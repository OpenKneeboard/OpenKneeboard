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
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabState.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <shims/winrt.h>
#include <wincodec.h>

using namespace OpenKneeboard;

namespace {

struct SharedTextureResources {
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<IDXGIKeyedMutex> mMutex;
  winrt::handle mHandle;
  UINT mMutexKey = 0;
};

}// namespace

namespace OpenKneeboard {

class InterprocessRenderer::Impl {
 public:
  HWND mFeederWindow;
  OpenKneeboard::SHM::Writer mSHM;
  DXResources mDXR;

  KneeboardState* mKneeboard = nullptr;

  bool mNeedsRepaint = true;

  // TODO: move to DXResources
  winrt::com_ptr<ID3D11DeviceContext> mD3DContext;
  winrt::com_ptr<ID3D11Texture2D> mCanvasTexture;
  winrt::com_ptr<ID2D1Bitmap1> mCanvasBitmap;

  std::array<SharedTextureResources, TextureCount> mResources;

  winrt::com_ptr<ID2D1Brush> mErrorBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderTextBrush;
  winrt::com_ptr<ID2D1Brush> mButtonBrush;
  winrt::com_ptr<ID2D1Brush> mHoverButtonBrush;
  winrt::com_ptr<ID2D1Brush> mActiveButtonBrush;
  winrt::com_ptr<ID2D1Brush> mCursorBrush;

  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;

  bool mCursorTouching = false;
  bool mCursorTouchOnNavButton;
  D2D1_RECT_F mNavButton;

  void RenderError(utf8_string_view tabTitle, utf8_string_view message);
  void CopyPixelsToSHM();
  void RenderWithChrome(
    const std::string_view tabTitle,
    const D2D1_SIZE_U& preferredContentSize,
    const std::function<void(const D2D1_RECT_F&)>& contentRenderer);

  void OnCursorEvent(const CursorEvent&);

 private:
  void RenderErrorImpl(utf8_string_view message, const D2D1_RECT_F&);
};

void InterprocessRenderer::Impl::RenderError(
  utf8_string_view tabTitle,
  utf8_string_view message) {
  this->RenderWithChrome(
    tabTitle,
    {768, 1024},
    std::bind_front(&Impl::RenderErrorImpl, this, message));
}

void InterprocessRenderer::Impl::RenderErrorImpl(
  utf8_string_view message,
  const D2D1_RECT_F& rect) {
  auto ctx = mDXR.mD2DDeviceContext.get();
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(rect, mErrorBGBrush.get());

  mErrorRenderer->Render(ctx, message, rect);
}

void InterprocessRenderer::Impl::CopyPixelsToSHM() {
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
    .feederWindow = mFeederWindow,
    .imageWidth = static_cast<uint16_t>(usedSize.width),
    .imageHeight = static_cast<uint16_t>(usedSize.height),
    .vr = mKneeboard->GetVRConfig(),
    .flat = mKneeboard->GetFlatConfig(),
  };
  mSHM.Update(config);
}

InterprocessRenderer::InterprocessRenderer(
  HWND feederWindow,
  const DXResources& dxr,
  KneeboardState* kneeboard)
  : p(std::make_unique<Impl>()) {
  p->mFeederWindow = feederWindow;
  p->mDXR = dxr;
  p->mKneeboard = kneeboard;
  p->mErrorRenderer
    = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext.get());

  dxr.mD3DDevice->GetImmediateContext(p->mD3DContext.put());

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
    &textureDesc, nullptr, p->mCanvasTexture.put()));

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    p->mCanvasTexture.as<IDXGISurface>().get(),
    nullptr,
    p->mCanvasBitmap.put()));

  textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE
    | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  for (auto i = 0; i < TextureCount; ++i) {
    auto& it = p->mResources.at(i);

    dxr.mD3DDevice->CreateTexture2D(&textureDesc, nullptr, it.mTexture.put());
    auto textureName = SHM::SharedTextureName(i);
    it.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
      nullptr,
      DXGI_SHARED_RESOURCE_READ,
      textureName.c_str(),
      it.mHandle.put());
    it.mMutex = it.mTexture.as<IDXGIKeyedMutex>();
  }

  auto ctx = dxr.mD2DDeviceContext;
  const auto vrconfig = kneeboard->GetVRConfig();

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mHeaderBGBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mHeaderTextBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mCursorBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.4f, 0.4f, 0.4f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mButtonBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mHoverButtonBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mActiveButtonBrush.put()));

  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, vrconfig.mBaseOpacity},
    reinterpret_cast<ID2D1SolidColorBrush**>(p->mErrorBGBrush.put()));

  AddEventListener(kneeboard->evCursorEvent, &Impl::OnCursorEvent, p.get());
  AddEventListener(
    kneeboard->evNeedsRepaintEvent, [this]() { p->mNeedsRepaint = true; });
  this->RenderNow();

  AddEventListener(kneeboard->evFrameTimerEvent, [this]() {
    if (p && p->mNeedsRepaint) {
      RenderNow();
    }
  });
}

InterprocessRenderer::~InterprocessRenderer() {
  if (!p) {
    return;
  }
  auto ctx = p->mD3DContext;
  p = {};
  ctx->Flush();
}

void InterprocessRenderer::Render(
  const std::shared_ptr<Tab>& tab,
  uint16_t pageIndex) {
  if (!tab) {
    auto msg = _("No Tab");
    p->RenderError(msg, msg);
    return;
  }

  const auto title = tab->GetTitle();
  const auto pageCount = tab->GetPageCount();
  if (pageCount == 0) {
    p->RenderError(title, _("No Pages"));
    return;
  }

  if (pageIndex >= pageCount) {
    p->RenderError(title, _("Invalid Page Number"));
    return;
  }

  const auto pageSize = tab->GetNativeContentSize(pageIndex);
  if (pageSize.width == 0 || pageSize.height == 0) {
    p->RenderError(title, _("Invalid Page Size"));
    return;
  }

  p->RenderWithChrome(
    title,
    pageSize,
    std::bind_front(
      &Tab::RenderPage, tab, p->mDXR.mD2DDeviceContext.get(), pageIndex));
}

void InterprocessRenderer::Impl::RenderWithChrome(
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

void InterprocessRenderer::Impl::OnCursorEvent(const CursorEvent& ev) {
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
  auto tabState = this->p->mKneeboard->GetCurrentTab();
  if (!tabState) {
    return;
  }
  this->Render(tabState->GetTab(), tabState->GetPageIndex());
  p->mNeedsRepaint = false;
}

}// namespace OpenKneeboard
