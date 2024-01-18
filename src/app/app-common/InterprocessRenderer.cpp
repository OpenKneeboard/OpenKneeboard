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
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/ToolbarAction.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
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

void InterprocessRenderer::Commit(uint8_t layerCount) noexcept {
  if (!mSHM) {
    return;
  }

  const auto tint = mKneeboard->GetAppSettings().mTint;

  const std::unique_lock dxLock(mDXR);
  const std::unique_lock shmLock(mSHM);

  std::vector<SHM::LayerConfig> shmLayers;

  auto ctx = mDXR.mD3DImmediateContext.get();

  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    auto& layer = mLayers.at(layerIndex);
    auto& it = layer.mSharedResources.at(mSHM.GetNextTextureIndex());

    if (tint.mEnabled) {
      D3D11::CopyTextureWithTint(
        mDXR.mD3DDevice.get(),
        layer.mCanvasSRV.get(),
        it.mTextureRTV.get(),
        {
          tint.mRed * tint.mBrightness,
          tint.mGreen * tint.mBrightness,
          tint.mBlue * tint.mBrightness,
          /* alpha = */ 1.0f,
        });
    } else {
      auto layerD3D = layer.mCanvas->d3d();
      D3D11_BOX box {
        0,
        0,
        0,
        layer.mConfig.mImageWidth,
        layer.mConfig.mImageHeight,
        1,
      };
      ctx->CopySubresourceRegion(
        it.mTexture.get(), 0, 0, 0, 0, layerD3D.texture(), 0, &box);
    }
    shmLayers.push_back(layer.mConfig);
  }

  const auto seq = mSHM.GetNextSequenceNumber();
  winrt::check_hresult(ctx->Signal(mFence.get(), seq));

  SHM::Config config {
    .mGlobalInputLayerID = mKneeboard->GetActiveViewForGlobalInput()
                             ->GetRuntimeID()
                             .GetTemporaryValue(),
    .mVR = mKneeboard->GetVRSettings(),
    .mFlat = mKneeboard->GetNonVRSettings(),
    .mTarget = GetConsumerPatternForGame(mCurrentGame),
  };

  mSHM.Update(config, shmLayers, mFenceHandle.get());
}

std::shared_ptr<InterprocessRenderer> InterprocessRenderer::Create(
  const DXResources& dxr,
  KneeboardState* kneeboard) {
  auto ret = shared_with_final_release(new InterprocessRenderer());
  ret->Init(dxr, kneeboard);
  return ret;
}

winrt::fire_and_forget InterprocessRenderer::final_release(
  std::unique_ptr<InterprocessRenderer> it) {
  // Delete in the correct thread
  co_await it->mOwnerThread;
}

std::mutex InterprocessRenderer::sSingleInstance;

InterprocessRenderer::InterprocessRenderer() : mInstanceLock(sSingleInstance) {
  dprint(__FUNCTION__);
}

void InterprocessRenderer::Init(
  const DXResources& dxr,
  KneeboardState* kneeboard) {
  auto currentGame = kneeboard->GetCurrentGame();
  if (currentGame) {
    mCurrentGame = currentGame->mGameInstance.lock();
  }

  mDXR = dxr;
  mKneeboard = kneeboard;

  const std::unique_lock d2dLock(mDXR);
  {
    winrt::com_ptr<ID3D11DeviceContext> ctx;
    dxr.mD3DDevice->GetImmediateContext(ctx.put());

    winrt::check_hresult(dxr.mD3DDevice->CreateFence(
      0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(mFence.put())));
    winrt::check_hresult(mFence->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, mFenceHandle.put()));
  }

  for (auto& layer: mLayers) {
    winrt::com_ptr<ID3D11Texture2D> canvasTexture {
      SHM::CreateCompatibleTexture(dxr.mD3DDevice.get())};

    winrt::check_hresult(dxr.mD3DDevice->CreateShaderResourceView(
      canvasTexture.get(), nullptr, layer.mCanvasSRV.put()));

    layer.mCanvas = RenderTarget::Create(mDXR, canvasTexture);
  }

  const auto sessionID = mSHM.GetSessionID();
  for (uint8_t layerIndex = 0; layerIndex < MaxLayers; ++layerIndex) {
    auto& layer = mLayers.at(layerIndex);
    for (uint8_t bufferIndex = 0; bufferIndex < TextureCount; ++bufferIndex) {
      auto& resources = layer.mSharedResources.at(bufferIndex);
      resources.mTexture = SHM::CreateCompatibleTexture(
        dxr.mD3DDevice.get(),
        SHM::DEFAULT_D3D11_BIND_FLAGS,
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED);
      winrt::check_hresult(dxr.mD3DDevice->CreateRenderTargetView(
        resources.mTexture.get(), nullptr, resources.mTextureRTV.put()));
      const auto textureName
        = mSHM.GetSharedTextureName(layerIndex, bufferIndex);
      winrt::check_hresult(
        resources.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
          nullptr,
          DXGI_SHARED_RESOURCE_READ,
          textureName.c_str(),
          resources.mSharedHandle.put()));
    }
  }

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
    mLayers.at(i).mKneeboardView = view;

    AddEventListener(view->evCursorEvent, markDirty);
  }

  this->RenderNow();

  AddEventListener(kneeboard->evFrameTimerEvent, weak_wrap(this)([](auto self) {
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
    const std::unique_lock d2dLock(mDXR);
    auto ctx = mDXR.mD2DDeviceContext.get();
    ctx->Flush();
    // De-allocate D3D resources while we have the lock
    mLayers = {};
    mFence = {};
  }
}

void InterprocessRenderer::Render(Layer& layer) {
  {
    auto d3d = layer.mCanvas->d3d();
    mDXR.mD3DImmediateContext->ClearRenderTargetView(
      d3d.rtv(), DirectX::Colors::Transparent);
  }

  const auto view = layer.mKneeboardView;
  layer.mConfig.mLayerID = view->GetRuntimeID().GetTemporaryValue();
  const auto usedSize = view->GetIPCRenderSize();
  layer.mConfig.mImageWidth = usedSize.width;
  layer.mConfig.mImageHeight = usedSize.height;

  const auto vrc = mKneeboard->GetVRSettings();
  const auto xFitScale = vrc.mMaxWidth / usedSize.width;
  const auto yFitScale = vrc.mMaxHeight / usedSize.height;
  const auto scale = std::min<float>(xFitScale, yFitScale);

  layer.mConfig.mVR.mWidth = usedSize.width * scale;
  layer.mConfig.mVR.mHeight = usedSize.height * scale;

  view->RenderWithChrome(
    layer.mCanvas.get(),
    {
      0,
      0,
      static_cast<FLOAT>(usedSize.width),
      static_cast<FLOAT>(usedSize.height),
    },
    layer.mIsActiveForInput);
}

void InterprocessRenderer::RenderNow() {
  if (mRendering.test_and_set()) {
    dprint("Two renders in the same instance");
    OPENKNEEBOARD_BREAK;
    return;
  }
  const scope_guard markDone([this]() { mRendering.clear(); });

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
  this->mNeedsRepaint = false;
}

void InterprocessRenderer::OnGameChanged(
  DWORD processID,
  const std::shared_ptr<GameInstance>& game) {
  mCurrentGame = game;
  this->MarkDirty();
}

}// namespace OpenKneeboard
