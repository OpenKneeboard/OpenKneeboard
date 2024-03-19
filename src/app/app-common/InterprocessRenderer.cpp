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
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/GameInstance.h>
#include <OpenKneeboard/GetSystemColor.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/Spriting.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/ToolbarAction.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/hresult.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/weak_wrap.h>

#include <mutex>
#include <ranges>

#include <DirectXColors.h>
#include <d3d11_4.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

namespace OpenKneeboard {

static SHM::ConsumerPattern GetConsumerPatternForGame(
  const std::shared_ptr<GameInstance>& game) {
  if (!game) {
    return {};
  }

  switch (game->mOverlayAPI) {
    case OverlayAPI::None:
      return {SHM::ConsumerKind::Viewer};
    case OverlayAPI::AutoDetect:
      return {};
    case OverlayAPI::SteamVR:
      return {SHM::ConsumerKind::SteamVR};
    case OverlayAPI::OpenXR:
      return {SHM::ConsumerKind::OpenXR};
    case OverlayAPI::OculusD3D11:
      return {SHM::ConsumerKind::OculusD3D11};
    case OverlayAPI::OculusD3D12:
      return {SHM::ConsumerKind::OculusD3D12};
    case OverlayAPI::NonVRD3D11:
      return {SHM::ConsumerKind::NonVRD3D11};
    default:
      dprintf(
        "Unhandled overlay API: {}",
        std::underlying_type_t<OverlayAPI>(game->mOverlayAPI));
      OPENKNEEBOARD_BREAK;
      return {};
  }
}

void InterprocessRenderer::SubmitFrame(
  const std::vector<SHM::LayerConfig>& shmLayers,
  uint64_t inputLayerID) noexcept {
  if (!mSHM) {
    return;
  }

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "InterprocessRenderer::SubmitFrame()");

  const auto layerCount = shmLayers.size();

  auto ctx = mDXR->mD3D11ImmediateContext.get();
  const D3D11_BOX srcBox {
    0,
    0,
    0,
    static_cast<UINT>(mCanvasSize.mWidth),
    static_cast<UINT>(mCanvasSize.mHeight),
    1,
  };

  auto srcTexture = mCanvas->d3d().texture();

  TraceLoggingWriteTagged(activity, "AcquireSHMLock/start");
  const std::unique_lock shmLock(mSHM);
  TraceLoggingWriteTagged(activity, "AcquireSHMLock/stop");

  auto ipcTextureInfo = mSHM.BeginFrame();
  auto destResources
    = this->GetIPCTextureResources(ipcTextureInfo.mTextureIndex, mCanvasSize);

  auto fence = destResources->mFence.get();
  {
    OPENKNEEBOARD_TraceLoggingScope(
      "CopyFromCanvas",
      TraceLoggingValue(ipcTextureInfo.mTextureIndex, "TextureIndex"),
      TraceLoggingValue(ipcTextureInfo.mFenceIn, "FenceIn"),
      TraceLoggingValue(ipcTextureInfo.mFenceOut, "FenceOut"));
    if (destResources->mNewFence) {
      destResources->mNewFence = false;
    } else {
      OPENKNEEBOARD_TraceLoggingScope("CopyFromCanvas/FenceIn");
      check_hresult(ctx->Wait(fence, ipcTextureInfo.mFenceIn));
    }
    {
      OPENKNEEBOARD_TraceLoggingScope("CopyFromCanvas/CopySubresourceRegion");
      ctx->CopySubresourceRegion(
        destResources->mTexture.get(), 0, 0, 0, 0, srcTexture, 0, &srcBox);
    }
    {
      OPENKNEEBOARD_TraceLoggingScope("CopyFromCanvas/FenceOut");
      check_hresult(ctx->Signal(fence, ipcTextureInfo.mFenceOut));
    }
  }

  SHM::Config config {
    .mGlobalInputLayerID = mKneeboard->GetActiveViewForGlobalInput()
                             ->GetRuntimeID()
                             .GetTemporaryValue(),
    .mVR = mKneeboard->GetVRSettings(),
    .mTarget = GetConsumerPatternForGame(mCurrentGame),
    .mTextureSize = destResources->mTextureSize,
  };
  const auto tint = mKneeboard->GetAppSettings().mTint;
  if (tint.mEnabled) {
    config.mTint = {
      tint.mRed * tint.mBrightness,
      tint.mGreen * tint.mBrightness,
      tint.mBlue * tint.mBrightness,
      /* alpha = */ 1.0f,
    };
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("SHMSubmitFrame");
    mSHM.SubmitFrame(
      config,
      shmLayers,
      destResources->mTextureHandle.get(),
      destResources->mFenceHandle.get());
  }
}

void InterprocessRenderer::InitializeCanvas(const PixelSize& size) {
  if (mCanvasSize == size) {
    return;
  }

  OPENKNEEBOARD_TraceLoggingScope("InterprocessRenderer::InitializeCanvas()");

  D3D11_TEXTURE2D_DESC desc {
    .Width = static_cast<UINT>(size.mWidth),
    .Height = static_cast<UINT>(size.mHeight),
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
  };

  auto device = mDXR->mD3D11Device.get();

  winrt::com_ptr<ID3D11Texture2D> texture;
  check_hresult(device->CreateTexture2D(&desc, nullptr, texture.put()));
  mCanvas
    = RenderTargetWithMultipleIdentities::Create(mDXR, texture, MaxViewCount);
  mCanvasSize = size;

  // Let's force a clean start on the clients, including resetting the session
  // ID
  mIPCSwapchain = {};
  const std::unique_lock shmLock(mSHM);
  mSHM.Detach();
}

void InterprocessRenderer::PostUserAction(UserAction action) {
  switch (action) {
    case UserAction::TOGGLE_VISIBILITY:
      mVisible = !mVisible;
      break;
    case UserAction::SHOW:
      mVisible = true;
      break;
    case UserAction::HIDE:
      mVisible = false;
      break;
    default:
      OPENKNEEBOARD_BREAK;
      return;
  }

  if (!mVisible) {
    mFirstInvisible = true;
  }

  mKneeboard->evNeedsRepaintEvent.Emit();
}

InterprocessRenderer::IPCTextureResources*
InterprocessRenderer::GetIPCTextureResources(
  uint8_t textureIndex,
  const PixelSize& size) {
  auto& ret = mIPCSwapchain.at(textureIndex);
  if (ret.mTextureSize == size) [[likely]] {
    return &ret;
  }

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity,
    "InterprocessRenderer::GetIPCTextureResources:",
    TraceLoggingValue(textureIndex, "textureIndex"),
    TraceLoggingValue(size.mWidth, "width"),
    TraceLoggingValue(size.mHeight, "height"));

  ret = {};

  auto device = mDXR->mD3D11Device.get();

  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = static_cast<UINT>(size.mWidth),
    .Height = static_cast<UINT>(size.mHeight),
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags
    = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED,
  };

  check_hresult(
    device->CreateTexture2D(&textureDesc, nullptr, ret.mTexture.put()));
  check_hresult(device->CreateRenderTargetView(
    ret.mTexture.get(), nullptr, ret.mRenderTargetView.put()));
  check_hresult(ret.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
    nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, ret.mTextureHandle.put()));

  TraceLoggingWriteTagged(activity, "Creating new fence");
  check_hresult(device->CreateFence(
    0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(ret.mFence.put())));
  check_hresult(ret.mFence->CreateSharedHandle(
    nullptr, GENERIC_ALL, nullptr, ret.mFenceHandle.put()));

  ret.mViewport = {
    0,
    0,
    static_cast<FLOAT>(size.mWidth),
    static_cast<FLOAT>(size.mHeight),
    0.0f,
    1.0f,
  };
  ret.mTextureSize = size;

  return &ret;
}

std::shared_ptr<InterprocessRenderer> InterprocessRenderer::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard) {
  auto ret = shared_with_final_release(new InterprocessRenderer(dxr));
  ret->Initialize(kneeboard);
  return ret;
}

winrt::fire_and_forget InterprocessRenderer::final_release(
  std::unique_ptr<InterprocessRenderer> it) {
  // Delete in the correct thread
  co_await it->mOwnerThread;
}

std::mutex InterprocessRenderer::sSingleInstance;

InterprocessRenderer::InterprocessRenderer(const audited_ptr<DXResources>& dxr)
  : mInstanceLock(sSingleInstance), mDXR(dxr), mSHM(dxr->mAdapterLUID) {
  dprint(__FUNCTION__);
}

void InterprocessRenderer::Initialize(KneeboardState* kneeboard) {
  auto currentGame = kneeboard->GetCurrentGame();
  if (currentGame) {
    mCurrentGame = currentGame->mGameInstance.lock();
  }

  mKneeboard = kneeboard;

  const auto markDirty = weak_wrap(this)([](auto self) { self->MarkDirty(); });

  AddEventListener(kneeboard->evNeedsRepaintEvent, markDirty);
  AddEventListener(
    kneeboard->evGameChangedEvent,
    [weak = weak_from_this()](DWORD pid, std::shared_ptr<GameInstance> game) {
      if (auto self = weak.lock()) {
        self->OnGameChanged(pid, game);
      }
    });

  const auto views = kneeboard->GetAllViewsInFixedOrder();
  for (int i = 0; i < views.size(); ++i) {
    auto view = views.at(i);
    AddEventListener(view->evCursorEvent, markDirty);
  }

  this->RenderNow();

  AddEventListener(kneeboard->evFrameTimerEvent, weak_wrap(this)([](auto self) {
                     OPENKNEEBOARD_TraceLoggingScope(
                       "InterprocessRenderer::evFrameTimerEvent callback");
                     if (self->mNeedsRepaint) {
                       self->RenderNow();
                     }
                   }));
}

void InterprocessRenderer::MarkDirty() {
  mNeedsRepaint = true;
}

InterprocessRenderer::~InterprocessRenderer() {
  dprint(__FUNCTION__);
  this->RemoveAllEventListeners();
  {
    // SHM::Writer's destructor will do this, but let's make sure to
    // tear it down before the vtable and other members go - especially
    // the D3D resources
    const std::unique_lock shmLock(mSHM);
    mSHM.Detach();
  }
  {
    const std::unique_lock d2dlock(*mDXR);
    auto ctx = mDXR->mD2DDeviceContext.get();
    ctx->Flush();
    // De-allocate D3D resources while we have the lock
    mIPCSwapchain = {};
    mCanvas = {};
  }
}

SHM::LayerConfig InterprocessRenderer::RenderLayer(
  const ViewRenderInfo& layer,
  const PixelRect& bounds) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("InterprocessRenderer::RenderLayer");
  const auto view = layer.mView.get();

  SHM::LayerConfig ret {};
  ret.mLayerID = view->GetRuntimeID().GetTemporaryValue();

  if (layer.mVR) {
    ret.mVREnabled = true;
    ret.mVR = *layer.mVR;
    ret.mVR.mLocationOnTexture.mOffset.mX += bounds.mOffset.mX;
    ret.mVR.mLocationOnTexture.mOffset.mY += bounds.mOffset.mY;
  }

  if (layer.mNonVR) {
    ret.mNonVREnabled = true;
    ret.mNonVR = *layer.mNonVR;
    ret.mNonVR.mLocationOnTexture.mOffset.mX += bounds.mOffset.mX;
    ret.mNonVR.mLocationOnTexture.mOffset.mY += bounds.mOffset.mY;
  }

  view->RenderWithChrome(
    mCanvas.get(),
    PixelRect {bounds.mOffset, layer.mFullSize},
    layer.mIsActiveForInput);

  return ret;
}

void InterprocessRenderer::RenderNow() noexcept {
  if (mRendering.test_and_set()) {
    dprint("Two renders in the same instance");
    OPENKNEEBOARD_BREAK;
    return;
  }

  const scope_guard markDone([this]() { mRendering.clear(); });

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "InterprocessRenderer::RenderNow()");

  if (!mVisible) {
    TraceLoggingWriteTagged(activity, "Invisible");
    if (mFirstInvisible) {
      mFirstInvisible = false;
      if (mSHM) {
        std::unique_lock lock(mSHM);
        mSHM.SubmitEmptyFrame();
      }
    }
    return;
  }

  const auto renderInfos = mKneeboard->GetViewRenderInfo();
  const auto layerCount = renderInfos.size();

  const auto canvasSize = Spriting::GetBufferSize(layerCount);

  TraceLoggingWriteTagged(activity, "AcquireDXLock/start");
  const std::unique_lock dxlock(*mDXR);
  TraceLoggingWriteTagged(activity, "AcquireDXLock/stop");
  this->InitializeCanvas(canvasSize);
  mDXR->mD3D11ImmediateContext->ClearRenderTargetView(
    mCanvas->d3d().rtv(), DirectX::Colors::Transparent);

  std::vector<SHM::LayerConfig> shmLayers;
  shmLayers.reserve(layerCount);
  uint64_t inputLayerID = 0;

  for (uint8_t i = 0; i < layerCount; ++i) {
    const auto bounds = Spriting::GetRect(i, layerCount);
    const auto& renderInfo = renderInfos.at(i);
    if (renderInfo.mIsActiveForInput) {
      inputLayerID = renderInfo.mView->GetRuntimeID().GetTemporaryValue();
    }

    mCanvas->SetActiveIdentity(i);

    shmLayers.push_back(this->RenderLayer(renderInfo, bounds));
  }

  this->SubmitFrame(shmLayers, inputLayerID);
  this->mNeedsRepaint = false;
}

void InterprocessRenderer::OnGameChanged(
  DWORD processID,
  const std::shared_ptr<GameInstance>& game) {
  mCurrentGame = game;
  this->MarkDirty();
}

}// namespace OpenKneeboard
