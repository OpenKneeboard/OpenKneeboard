/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabState.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

using namespace OpenKneeboard;

namespace OpenKneeboard {

void InterprocessRenderer::RenderError(
  utf8_string_view tabTitle,
  utf8_string_view message) {
  this->RenderWithChrome(
    tabTitle,
    {768, 1024},
    std::bind_front(&InterprocessRenderer::RenderErrorImpl, this, message));
}

void InterprocessRenderer::RenderErrorImpl(
  utf8_string_view message,
  const D2D1_RECT_F& rect) {
  auto ctx = mDXR.mD2DDeviceContext.get();
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(rect, mErrorBGBrush.get());

  mErrorRenderer->Render(ctx, message, rect);
}

void InterprocessRenderer::CopyPixelsToSHM() {
  if (!mSHM) {
    return;
  }

  mD3DContext->Flush();

  auto& it = mResources.at(mSHM.GetNextTextureIndex());

  it.mMutex->AcquireSync(it.mMutexKey, INFINITE);
  mD3DContext->CopyResource(it.mTexture.get(), mCanvasTexture.get());
  mD3DContext->Flush();
  it.mMutexKey = mSHM.GetNextTextureKey();
  it.mMutex->ReleaseSync(it.mMutexKey);

  auto usedSize = mKneeboard->GetCanvasSize();

  SHM::Config config {
    .mFeederWindow = mFeederWindow,
    .mImageWidth = static_cast<uint16_t>(usedSize.width),
    .mImageHeight = static_cast<uint16_t>(usedSize.height),
    .mVR = mKneeboard->GetVRConfig(),
    .mFlat = mKneeboard->GetFlatConfig(),
  };
  mSHM.Update(config);
}

InterprocessRenderer::InterprocessRenderer(
  HWND feederWindow,
  const DXResources& dxr,
  KneeboardState* kneeboard) {
  mFeederWindow = feederWindow;
  mDXR = dxr;
  mKneeboard = kneeboard;
  mErrorRenderer
    = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext.get());

  dxr.mD3DDevice->GetImmediateContext(mD3DContext.put());

  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags = 0,
  };
  winrt::check_hresult(dxr.mD3DDevice->CreateTexture2D(
    &textureDesc, nullptr, mCanvasTexture.put()));

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    mCanvasTexture.as<IDXGISurface>().get(), nullptr, mCanvasBitmap.put()));

  textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE
    | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  const auto sessionID = mSHM.GetSessionID();
  for (auto i = 0; i < TextureCount; ++i) {
    auto& it = mResources.at(i);

    dxr.mD3DDevice->CreateTexture2D(&textureDesc, nullptr, it.mTexture.put());
    auto textureName = SHM::SharedTextureName(sessionID, i);
    it.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
      nullptr,
      DXGI_SHARED_RESOURCE_READ,
      textureName.c_str(),
      it.mHandle.put());
    it.mMutex = it.mTexture.as<IDXGIKeyedMutex>();
  }

  auto ctx = dxr.mD2DDeviceContext;

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHeaderBGBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHeaderTextBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mCursorBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.4f, 0.4f, 0.4f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mButtonBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHoverButtonBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mActiveButtonBrush.put()));

  ctx->CreateSolidColorBrush(
    GetSystemColor(COLOR_WINDOW),
    reinterpret_cast<ID2D1SolidColorBrush**>(mErrorBGBrush.put()));

  AddEventListener(
    kneeboard->evCursorEvent, &InterprocessRenderer::OnCursorEvent, this);
  AddEventListener(
    kneeboard->evNeedsRepaintEvent, [this]() { mNeedsRepaint = true; });
  this->RenderNow();

  AddEventListener(kneeboard->evFrameTimerEvent, [this]() {
    if (mNeedsRepaint) {
      RenderNow();
    }
  });
}

InterprocessRenderer::~InterprocessRenderer() {
  auto ctx = mD3DContext;
  ctx->Flush();
}

void InterprocessRenderer::Render(
  const std::shared_ptr<Tab>& tab,
  uint16_t pageIndex) {
  if (!tab) {
    auto msg = _("No Tab");
    this->RenderError(msg, msg);
    return;
  }

  const auto title = tab->GetTitle();
  const auto pageCount = tab->GetPageCount();
  if (pageCount == 0) {
    this->RenderError(title, _("No Pages"));
    return;
  }

  if (pageIndex >= pageCount) {
    this->RenderError(title, _("Invalid Page Number"));
    return;
  }

  const auto pageSize = tab->GetNativeContentSize(pageIndex);
  if (pageSize.width == 0 || pageSize.height == 0) {
    this->RenderError(title, _("Invalid Page Size"));
    return;
  }

  this->RenderWithChrome(
    title,
    pageSize,
    std::bind_front(
      &Tab::RenderPage, tab, mDXR.mD2DDeviceContext.get(), pageIndex));
}

void InterprocessRenderer::RenderWithChrome(
  std::string_view tabTitle,
  const D2D1_SIZE_U& preferredContentSize,
  const std::function<void(const D2D1_RECT_F&)>& renderContent) {
  auto ctx = mDXR.mD2DDeviceContext;
  ctx->SetTarget(mCanvasBitmap.get());

  ctx->BeginDraw();
  const auto cleanup = scope_guard([&ctx, this]() {
    winrt::check_hresult(ctx->EndDraw());
    this->CopyPixelsToSHM();
  });
  ctx->Clear({0.0f, 0.0f, 0.0f, 0.0f});

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  renderContent(mKneeboard->GetContentRenderRect());

  const auto headerRect = mKneeboard->GetHeaderRenderRect();

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(headerRect, mHeaderBGBrush.get());

  const D2D1_SIZE_F headerSize {
    headerRect.right - headerRect.left,
    headerRect.bottom - headerRect.top,
  };

  FLOAT dpix, dpiy;
  ctx->GetDpi(&dpix, &dpiy);

  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  auto dwf = mDXR.mDWriteFactory;
  dwf->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (headerSize.height * 96) / (2 * dpiy),
    L"",
    headerFormat.put());

  auto title = winrt::to_hstring(tabTitle);
  winrt::com_ptr<IDWriteTextLayout> headerLayout;
  dwf->CreateTextLayout(
    title.data(),
    static_cast<UINT32>(title.size()),
    headerFormat.get(),
    float(headerSize.width),
    float(headerSize.height),
    headerLayout.put());
  headerLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  headerLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  ctx->DrawTextLayout({0.0f, 0.0f}, headerLayout.get(), mHeaderTextBrush.get());

#ifdef DEBUG
  auto frameText = fmt::format(L"Frame {}", mSHM.GetNextSequenceNumber());
  headerFormat = nullptr;
  dwf->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    0.67f * (headerSize.height * 96) / (2 * dpiy),
    L"",
    headerFormat.put());
  headerLayout = nullptr;
  dwf->CreateTextLayout(
    frameText.data(),
    static_cast<UINT32>(frameText.size()),
    headerFormat.get(),
    float(headerSize.width),
    float(headerSize.height),
    headerLayout.put());
  headerLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  headerLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
  ctx->DrawTextLayout({0.0f, 0.0f}, headerLayout.get(), mHeaderTextBrush.get());
#endif

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  auto cursorPoint = mKneeboard->GetCursorCanvasPoint();

  const auto tab = mKneeboard->GetCurrentTab();
  if (tab->SupportsTabMode(TabMode::NAVIGATION)) {
    const auto buttonHeight = headerSize.height * 0.75f;
    const auto margin = (headerSize.height - buttonHeight) / 2.0f;

    mNavButton = {
      2 * margin,
      margin,
      buttonHeight + (2 * margin),
      buttonHeight + margin,
    };

    ID2D1Brush* brush = mButtonBrush.get();
    if (
      cursorPoint.x >= mNavButton.left && cursorPoint.x <= mNavButton.right
      && cursorPoint.y >= mNavButton.top
      && cursorPoint.y <= mNavButton.bottom) {
      brush = mHoverButtonBrush.get();
    } else if (tab->GetTabMode() == TabMode::NAVIGATION) {
      brush = mActiveButtonBrush.get();
    }

    const auto strokeWidth = buttonHeight / 15;

    ctx->DrawRoundedRectangle(
      D2D1::RoundedRect(mNavButton, buttonHeight / 4, buttonHeight / 4),
      brush,
      strokeWidth);

    // Half a stroke width on top and bottom
    const auto innerHeight = buttonHeight - strokeWidth;
    const auto step = innerHeight / 4;
    const auto padding = (strokeWidth / 2) + step;
    D2D1_POINT_2F left {
      mNavButton.left + padding,
      mNavButton.top + padding,
    };
    D2D1_POINT_2F right {
      mNavButton.right - padding,
      mNavButton.top + padding,
    };
    for (auto i = 0; i <= 2; ++i) {
      ctx->DrawLine(left, right, brush, step / 2);
      left.y += step;
      right.y += step;
    }
  }

  if (!mKneeboard->HaveCursor()) {
    return;
  }

  const auto contentRect = mKneeboard->GetContentRenderRect();
  const D2D1_SIZE_F contentSize = {
    contentRect.right - contentRect.left,
    contentRect.bottom - contentRect.top,
  };

  const auto cursorRadius = contentSize.height / CursorRadiusDivisor;
  const auto cursorStroke = contentSize.height / CursorStrokeDivisor;
  ctx->DrawEllipse(
    D2D1::Ellipse(cursorPoint, cursorRadius, cursorRadius),
    mCursorBrush.get(),
    cursorStroke);
}

void InterprocessRenderer::OnCursorEvent(const CursorEvent& ev) {
  mNeedsRepaint = true;
  const auto tab = mKneeboard->GetCurrentTab();
  if (!tab->SupportsTabMode(TabMode::NAVIGATION)) {
    return;
  }
  if (mCursorTouching && ev.mTouchState == CursorTouchState::TOUCHING_SURFACE) {
    return;
  }
  if (
    (!mCursorTouching)
    && ev.mTouchState != CursorTouchState::TOUCHING_SURFACE) {
    return;
  }

  // touch state change
  mCursorTouching = (ev.mTouchState == CursorTouchState::TOUCHING_SURFACE);

  const auto point = mKneeboard->GetCursorCanvasPoint({ev.mX, ev.mY});
  const auto pointInNavButton = point.x >= mNavButton.left
    && point.x <= mNavButton.right && point.y >= mNavButton.left
    && point.y <= mNavButton.right;

  if (mCursorTouching) {
    // Touch start
    mCursorTouchOnNavButton = pointInNavButton;
    return;
  }

  // Touch end
  if (!mCursorTouchOnNavButton) {
    return;
  }
  if (pointInNavButton) {
    if (tab->GetTabMode() == TabMode::NAVIGATION) {
      tab->SetTabMode(TabMode::NORMAL);
    } else {
      tab->SetTabMode(TabMode::NAVIGATION);
    }
  }
}

void InterprocessRenderer::RenderNow() {
  auto tabState = this->mKneeboard->GetCurrentTab();
  if (!tabState) {
    return;
  }
  this->Render(tabState->GetTab(), tabState->GetPageIndex());
  mNeedsRepaint = false;
}

}// namespace OpenKneeboard
