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
#include <OpenKneeboard/CreateTabActions.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabAction.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

using namespace OpenKneeboard;

namespace OpenKneeboard {

void InterprocessRenderer::RenderError(
  Layer& layer,
  utf8_string_view tabTitle,
  utf8_string_view message) {
  this->RenderWithChrome(
    layer,
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

void InterprocessRenderer::Commit(uint8_t layerCount) {
  if (!mSHM) {
    return;
  }

  mD3DContext->Flush();

  std::vector<SHM::LayerConfig> shmLayers;

  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    auto& layer = mLayers.at(layerIndex);
    auto& it = layer.mSharedResources.at(mSHM.GetNextTextureIndex());

    it.mMutex->AcquireSync(it.mMutexKey, INFINITE);
    mD3DContext->CopyResource(it.mTexture.get(), layer.mCanvasTexture.get());
    mD3DContext->Flush();
    it.mMutexKey = mSHM.GetNextTextureKey();
    it.mMutex->ReleaseSync(it.mMutexKey);

    shmLayers.push_back(layer.mConfig);
  }

  SHM::Config config {
    .mVR = mKneeboard->GetVRConfig(),
    .mFlat = mKneeboard->GetFlatConfig(),
  };

  mSHM.Update(config, shmLayers);
}

InterprocessRenderer::InterprocessRenderer(
  const DXResources& dxr,
  KneeboardState* kneeboard) {
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

  for (auto& layer: mLayers) {
    winrt::check_hresult(dxr.mD3DDevice->CreateTexture2D(
      &textureDesc, nullptr, layer.mCanvasTexture.put()));

    winrt::check_hresult(dxr.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
      layer.mCanvasTexture.as<IDXGISurface>().get(),
      nullptr,
      layer.mCanvasBitmap.put()));
  }

  textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE
    | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  const auto sessionID = mSHM.GetSessionID();
  for (uint8_t layerIndex = 0; layerIndex < MaxLayers; ++layerIndex) {
    auto& layer = mLayers.at(layerIndex);
    for (uint8_t bufferIndex = 0; bufferIndex < TextureCount; ++bufferIndex) {
      auto& resources = layer.mSharedResources.at(bufferIndex);
      winrt::check_hresult(dxr.mD3DDevice->CreateTexture2D(
        &textureDesc, nullptr, resources.mTexture.put()));
      auto textureName
        = SHM::SharedTextureName(sessionID, layerIndex, bufferIndex);
      winrt::check_hresult(
        resources.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
          nullptr,
          DXGI_SHARED_RESOURCE_READ,
          textureName.c_str(),
          resources.mHandle.put()));
      resources.mMutex = resources.mTexture.as<IDXGIKeyedMutex>();
    }
  }

  auto ctx = dxr.mD2DDeviceContext;

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.8f},
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
    {0.4f, 0.4f, 0.4f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mDisabledButtonBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
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
    kneeboard->evNeedsRepaintEvent, [this]() { mNeedsRepaint = true; });

  const auto views = kneeboard->GetAllViewsInFixedOrder();
  for (int i = 0; i < views.size(); ++i) {
    auto view = views.at(i);
    mLayers.at(i).mKneeboardView = view;

    const std::weak_ptr<IKneeboardView> weakView(view);
    AddEventListener(view->evCursorEvent, [=](const CursorEvent& ev) {
      this->OnCursorEvent(weakView, ev);
    });
    this->AddEventListener(
      view->evCurrentTabChangedEvent, [=]() { this->OnTabChanged(weakView); });
    AddEventListener(
      view->evNeedsRepaintEvent, [this]() { mNeedsRepaint = true; });
    this->OnTabChanged(weakView);
  }

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

void InterprocessRenderer::Render(Layer& layer) {
  const auto view = layer.mKneeboardView;
  const auto tabView = view->GetCurrentTabView();
  const auto tab = tabView->GetTab();
  const auto pageIndex = tabView->GetPageIndex();

  const auto usedSize = view->GetCanvasSize();
  layer.mConfig.mImageWidth = usedSize.width;
  layer.mConfig.mImageHeight = usedSize.height;

  auto ctx = mDXR.mD2DDeviceContext;
  ctx->SetTarget(layer.mCanvasBitmap.get());
  ctx->BeginDraw();
  const scope_guard endDraw {
    [&, this]() { winrt::check_hresult(ctx->EndDraw()); }};
  ctx->Clear({0.0f, 0.0f, 0.0f, 0.0f});
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());

  if (!tab) {
    auto msg = _("No Tab");
    this->RenderError(layer, msg, msg);
    return;
  }

  const auto title = tab->GetTitle();
  const auto pageCount = tab->GetPageCount();
  if (pageCount == 0) {
    this->RenderError(layer, title, _("No Pages"));
    return;
  }

  if (pageIndex >= pageCount) {
    this->RenderError(layer, title, _("Invalid Page Number"));
    return;
  }

  const auto pageSize = tab->GetNativeContentSize(pageIndex);
  if (pageSize.width == 0 || pageSize.height == 0) {
    this->RenderError(layer, title, _("Invalid Page Size"));
    return;
  }

  this->RenderWithChrome(
    layer,
    title,
    pageSize,
    std::bind_front(
      &Tab::RenderPage, tab, mDXR.mD2DDeviceContext.get(), pageIndex));
}

void InterprocessRenderer::RenderWithChrome(
  Layer& layer,
  std::string_view tabTitle,
  const D2D1_SIZE_U& preferredContentSize,
  const std::function<void(const D2D1_RECT_F&)>& renderContent) {
  const auto view = layer.mKneeboardView;
  const auto contentRect = view->GetContentRenderRect();
  renderContent(contentRect);

  const auto headerRect = view->GetHeaderRenderRect();

  auto ctx = mDXR.mD2DDeviceContext;
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
  auto frameText = std::format(L"Frame {}", mSHM.GetNextSequenceNumber());
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
  auto cursorPoint = view->GetCursorCanvasPoint();

  this->RenderToolbar(layer);

  if (!view->HaveCursor()) {
    return;
  }

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

static bool IsPointInRect(const D2D1_POINT_2F& p, const D2D1_RECT_F& r) {
  return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
}

void InterprocessRenderer::RenderToolbar(Layer& layer) {
  if (!layer.mIsActiveForInput) {
    return;
  }

  const auto view = layer.mKneeboardView;
  const auto header = view->GetHeaderRenderRect();
  const auto headerHeight = (header.bottom - header.top);
  const auto buttonHeight = headerHeight * 0.75f;
  const auto margin = (headerHeight - buttonHeight) / 2.0f;

  const auto strokeWidth = buttonHeight / 15;

  auto left = 2 * margin;

  std::scoped_lock lock(layer.mToolbarMutex);

  layer.mButtons.clear();

  const auto cursor = view->GetCursorCanvasPoint();
  const auto ctx = mDXR.mD2DDeviceContext;

  FLOAT dpix, dpiy;
  ctx->GetDpi(&dpix, &dpiy);
  winrt::com_ptr<IDWriteTextFormat> glyphFormat;
  winrt::check_hresult(mDXR.mDWriteFactory->CreateTextFormat(
    L"Segoe MDL2 Assets",
    nullptr,
    DWRITE_FONT_WEIGHT_EXTRA_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (buttonHeight * 96) * 0.66f / dpiy,
    L"en-us",
    glyphFormat.put()));
  glyphFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  glyphFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  for (auto action: layer.mActions) {
    D2D1_RECT_F button {
      .left = left,
      .top = margin,
      .right = left + buttonHeight,
      .bottom = margin + buttonHeight,
    };
    left = button.right + margin;
    layer.mButtons.push_back({button, action});

    ID2D1Brush* brush = mButtonBrush.get();
    if (!action->IsEnabled()) {
      brush = mDisabledButtonBrush.get();
    } else if (IsPointInRect(cursor, button)) {
      brush = mHoverButtonBrush.get();
    } else {
      auto toggle = std::dynamic_pointer_cast<TabToggleAction>(action);
      if (toggle && toggle->IsActive()) {
        brush = mActiveButtonBrush.get();
      }
    }

    ctx->DrawRoundedRectangle(
      D2D1::RoundedRect(button, buttonHeight / 4, buttonHeight / 4),
      brush,
      strokeWidth);
    auto glyph = winrt::to_hstring(action->GetGlyph());
    ctx->DrawTextW(
      glyph.c_str(), glyph.size(), glyphFormat.get(), button, brush);
  }
}

void InterprocessRenderer::OnCursorEvent(
  const std::weak_ptr<IKneeboardView>& weakView,
  const CursorEvent& ev) {
  const auto view = weakView.lock();
  if (!view) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  auto it = std::ranges::find(
    mLayers, view, [](const Layer& l) { return l.mKneeboardView; });
  if (it == mLayers.end()) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  auto& layer = *it;

  mNeedsRepaint = true;
  const auto tab = view->GetCurrentTabView();
  if (
    layer.mCursorTouching
    && ev.mTouchState == CursorTouchState::TOUCHING_SURFACE) {
    return;
  }
  if (
    (!layer.mCursorTouching)
    && ev.mTouchState != CursorTouchState::TOUCHING_SURFACE) {
    return;
  }

  // touch state change
  layer.mCursorTouching
    = (ev.mTouchState == CursorTouchState::TOUCHING_SURFACE);

  const auto cursor = view->GetCursorCanvasPoint({ev.mX, ev.mY});

  std::scoped_lock lock(layer.mToolbarMutex);

  if (layer.mCursorTouching) {
    // Touch start
    for (const auto& [rect, action]: layer.mButtons) {
      if (IsPointInRect(cursor, rect)) {
        layer.mActiveButton = {{rect, action}};
        break;
      }
    }
    return;
  }

  // Touch end
  const auto button = layer.mActiveButton;
  if (!button) {
    return;
  }

  const auto [rect, action] = *button;
  layer.mActiveButton = {};
  if (!IsPointInRect(cursor, rect)) {
    return;
  }

  action->Execute();
}

void InterprocessRenderer::OnTabChanged(
  const std::weak_ptr<IKneeboardView>& weakView) {
  const auto view = weakView.lock();
  if (!view) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  auto it = std::ranges::find(
    mLayers, view, [](const Layer& l) { return l.mKneeboardView; });
  if (it == mLayers.end()) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  auto& layer = *it;

  auto tab = view->GetCurrentTabView();
  if (!tab) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  std::scoped_lock lock(layer.mToolbarMutex);
  layer.mActions = CreateTabActions(mKneeboard, tab);
  layer.mButtons.clear();
  layer.mActiveButton = {};

  for (auto action: layer.mActions) {
    AddEventListener(
      action->evStateChangedEvent, [this]() { mNeedsRepaint = true; });
  }
}

void InterprocessRenderer::RenderNow() {
  const auto renderInfos = mKneeboard->GetViewRenderInfo();

  for (uint8_t i = 0; i < renderInfos.size(); ++i) {
    auto& layer = mLayers.at(i);
    const auto& info = renderInfos.at(i);
    layer.mKneeboardView = info.mView;
    layer.mConfig.mVR = info.mVR;
    layer.mIsActiveForInput = info.mIsActiveForInput;
    this->Render(layer);
  }

  this->Commit(renderInfos.size());
  mNeedsRepaint = false;
}

}// namespace OpenKneeboard
