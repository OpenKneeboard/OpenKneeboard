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
#include <OpenKneeboard/CursorClickableRegions.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/CursorRenderer.h>
#include <OpenKneeboard/GameInstance.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/TabAction.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

#include <ranges>

namespace OpenKneeboard {

static SHM::ConsumerPattern GetConsumerPatternForGame(
  const std::shared_ptr<GameInstance>& game) {
  if (!game) {
    return {};
  }

  switch (game->mOverlayAPI) {
    case OverlayAPI::AutoDetect:
      return {};
      break;
    case OverlayAPI::SteamVR:
      return {SHM::ConsumerKind::SteamVR};
      break;
    case OverlayAPI::OpenXR:
      return {SHM::ConsumerKind::OpenXR};
      break;
    case OverlayAPI::OculusD3D11:
      return {SHM::ConsumerKind::OculusD3D11};
      break;
    case OverlayAPI::OculusD3D12:
      return {SHM::ConsumerKind::OculusD3D12};
      break;
    case OverlayAPI::NonVRD3D11:
      return {SHM::ConsumerKind::NonVRD3D11};
      break;
    default:
      dprintf(
        "Unhandled overlay API: {}",
        std::underlying_type_t<OverlayAPI>(game->mOverlayAPI));
      OPENKNEEBOARD_BREAK;
      return {};
  }
}

bool InterprocessRenderer::Button::operator==(
  const Button& other) const noexcept {
  return mAction == other.mAction;
}

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
    .mGlobalInputLayerID = mKneeboard->GetActiveViewForGlobalInput()
                             ->GetRuntimeID()
                             .GetTemporaryValue(),
    .mVR = mKneeboard->GetVRConfig(),
    .mFlat = mKneeboard->GetFlatConfig(),
    .mTarget = GetConsumerPatternForGame(mCurrentGame),
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
  mCursorRenderer = std::make_unique<CursorRenderer>(dxr);

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
  AddEventListener(
    kneeboard->evGameChangedEvent,
    std::bind_front(&InterprocessRenderer::OnGameChanged, this));

  const auto views = kneeboard->GetAllViewsInFixedOrder();
  for (int i = 0; i < views.size(); ++i) {
    auto view = views.at(i);
    mLayers.at(i).mKneeboardView = view;

    const std::weak_ptr<IKneeboardView> weakView(view);
    AddEventListener(view->evCursorEvent, [=](const CursorEvent& ev) {
      this->OnCursorEvent(weakView, ev);
    });
    AddEventListener(
      view->evLayoutChangedEvent, [=]() { this->OnLayoutChanged(weakView); });
    AddEventListener(
      view->evNeedsRepaintEvent, [this]() { mNeedsRepaint = true; });
    this->OnLayoutChanged(weakView);
  }

  AddEventListener(kneeboard->evFrameTimerEvent, [this]() {
    if (mNeedsRepaint) {
      RenderNow();
    }
  });
}

InterprocessRenderer::~InterprocessRenderer() {
  this->RemoveAllEventListeners();
  auto ctx = mD3DContext;
  ctx->Flush();
}

void InterprocessRenderer::Render(Layer& layer) {
  auto ctx = mDXR.mD2DDeviceContext;
  ctx->SetTarget(layer.mCanvasBitmap.get());
  ctx->BeginDraw();
  const scope_guard endDraw {
    [&, this]() { winrt::check_hresult(ctx->EndDraw()); }};
  ctx->Clear({0.0f, 0.0f, 0.0f, 0.0f});
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());

  const auto view = layer.mKneeboardView;
  layer.mConfig.mLayerID = view->GetRuntimeID().GetTemporaryValue();
  const auto usedSize = view->GetCanvasSize();
  layer.mConfig.mImageWidth = usedSize.width;
  layer.mConfig.mImageHeight = usedSize.height;

  const auto vrc = mKneeboard->GetVRConfig();
  const auto xFitScale = vrc.mMaxWidth / usedSize.width;
  const auto yFitScale = vrc.mMaxHeight / usedSize.height;
  const auto scale = std::min<float>(xFitScale, yFitScale);
  layer.mConfig.mVR.mWidth = usedSize.width * scale;
  layer.mConfig.mVR.mHeight = usedSize.height * scale;

  const auto tabView = view->GetCurrentTabView();
  if (!tabView) {
    auto msg = _("No Tab");
    this->RenderError(layer, msg, msg);
    return;
  }

  const auto tab = tabView->GetTab();

  if (!tab) {
    auto msg = _("No Tab");
    this->RenderError(layer, msg, msg);
    return;
  }

  const auto pageIndex = tabView->GetPageIndex();

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
      &ITab::RenderPage, tab, mDXR.mD2DDeviceContext.get(), pageIndex));
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

  auto cursorPoint = view->GetCursorCanvasPoint();

  this->RenderToolbar(layer);

  if (!view->HaveCursor()) {
    return;
  }

  const D2D1_SIZE_F contentSize = {
    contentRect.right - contentRect.left,
    contentRect.bottom - contentRect.top,
  };

  mCursorRenderer->Render(ctx.get(), cursorPoint, contentSize);
}

void InterprocessRenderer::RenderToolbar(Layer& layer) {
  if (!layer.mIsActiveForInput) {
    return;
  }

  const auto view = layer.mKneeboardView;
  const auto toolbar = mToolbars[view->GetRuntimeID()];
  if (!toolbar) {
    return;
  }

  const auto [hoverButton, buttons] = toolbar->GetState();
  if (buttons.empty()) {
    return;
  }

  const auto cursor = view->GetCursorCanvasPoint();
  const auto ctx = mDXR.mD2DDeviceContext;

  const auto buttonHeight
    = buttons.front().mRect.bottom - buttons.front().mRect.top;
  const auto strokeWidth = buttonHeight / 15;

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

  for (auto button: buttons) {
    auto& action = button.mAction;
    ID2D1Brush* brush = mButtonBrush.get();
    if (!action->IsEnabled()) {
      brush = mDisabledButtonBrush.get();
    } else if (hoverButton == button) {
      brush = mHoverButtonBrush.get();
    } else {
      auto toggle = std::dynamic_pointer_cast<TabToggleAction>(action);
      if (toggle && toggle->IsActive()) {
        brush = mActiveButtonBrush.get();
      }
    }

    ctx->DrawRoundedRectangle(
      D2D1::RoundedRect(button.mRect, buttonHeight / 4, buttonHeight / 4),
      brush,
      strokeWidth);
    auto glyph = winrt::to_hstring(action->GetGlyph());
    ctx->DrawTextW(
      glyph.c_str(), glyph.size(), glyphFormat.get(), button.mRect, brush);
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

  auto toolbar = mToolbars[view->GetRuntimeID()];
  if (!toolbar) {
    return;
  }
  scope_guard repaintOnExit([this]() { mNeedsRepaint = true; });

  const auto cursor = view->GetCursorCanvasPoint({ev.mX, ev.mY});
  CursorEvent canvasEvent {ev};
  canvasEvent.mX = cursor.x;
  canvasEvent.mY = cursor.y;
  toolbar->PostCursorEvent(mEventContext, canvasEvent);
}

void InterprocessRenderer::OnLayoutChanged(
  const std::weak_ptr<IKneeboardView>& weakView) {
  const auto view = weakView.lock();
  if (!view) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  auto tab = view->GetCurrentTabView();
  if (!tab) {
    return;
  }

  auto allActions = CreateTabActions(mKneeboard, view, tab);
  std::vector<std::shared_ptr<TabAction>> actions;

  std::ranges::copy_if(
    allActions, std::back_inserter(actions), [](const auto& action) {
      return action->GetVisibility(TabAction::Context::InGameToolbar)
        == TabAction::Visibility::Primary;
    });
  std::ranges::copy_if(
    allActions | std::ranges::views::reverse,
    std::back_inserter(actions),
    [](const auto& action) {
      return action->GetVisibility(TabAction::Context::InGameToolbar)
        == TabAction::Visibility::Secondary;
    });
  allActions.clear();

  std::vector<Button> buttons;

  const auto header = view->GetHeaderRenderRect();
  const auto headerHeight = (header.bottom - header.top);
  const auto buttonHeight = headerHeight * 0.75f;
  const auto margin = (headerHeight - buttonHeight) / 2.0f;

  auto primaryLeft = 2 * margin;
  auto secondaryRight = header.right - primaryLeft;

  for (const auto& action: actions) {
    const auto visibility
      = action->GetVisibility(TabAction::Context::InGameToolbar);
    if (visibility == TabAction::Visibility::None) [[unlikely]] {
      throw std::logic_error("Should not have been copied to actions local");
    }

    AddEventListener(
      action->evStateChangedEvent, [this]() { mNeedsRepaint = true; });

    D2D1_RECT_F button {
      .top = margin,
      .bottom = margin + buttonHeight,
    };
    if (visibility == TabAction::Visibility::Primary) {
      button.left = primaryLeft;
      button.right = primaryLeft + buttonHeight,
      primaryLeft = button.right + margin;
    } else {
      button.right = secondaryRight;
      button.left = secondaryRight - buttonHeight;
      secondaryRight = button.left - margin;
    }
    buttons.push_back({button, action});
  }

  auto toolbar = std::make_shared<Toolbar>(buttons);
  AddEventListener(toolbar->evClicked, [](auto, const Button& button) {
    button.mAction->Execute();
  });
  mToolbars[view->GetRuntimeID()] = toolbar;
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

void InterprocessRenderer::OnGameChanged(
  DWORD processID,
  const std::shared_ptr<GameInstance>& game) {
  mCurrentGame = game;
  this->mNeedsRepaint = true;
}

}// namespace OpenKneeboard
