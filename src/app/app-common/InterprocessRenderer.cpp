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
#include <d3d11_4.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

#include <mutex>
#include <ranges>

namespace OpenKneeboard {

static SHM::ConsumerPattern GetConsumerPatternForGame(
  const std::shared_ptr<GameInstance>& game) {
  if (!game) {
    return {};
  }

  switch (game->mOverlayAPI) {
    case OverlayAPI::None:
      return {SHM::ConsumerKind::Test};
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
      D3D11_BOX box {
        0,
        0,
        0,
        layer.mConfig.mImageWidth,
        layer.mConfig.mImageHeight,
        1,
      };
      mD3DContext->CopySubresourceRegion(
        it.mTexture.get(), 0, 0, 0, 0, layer.mCanvasTexture.get(), 0, &box);
    }
    shmLayers.push_back(layer.mConfig);
  }

  const auto seq = mSHM.GetNextSequenceNumber();
  winrt::check_hresult(mD3DContext->Signal(mFence.get(), seq));

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

InterprocessRenderer::InterprocessRenderer(
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
    mD3DContext = ctx.as<ID3D11DeviceContext4>();

    winrt::check_hresult(dxr.mD3DDevice.as<ID3D11Device5>()->CreateFence(
      0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(mFence.put())));
    winrt::check_hresult(mFence->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, mFenceHandle.put()));
  }

  for (auto& layer: mLayers) {
    layer.mCanvasTexture = SHM::CreateCompatibleTexture(dxr.mD3DDevice.get());

    winrt::check_hresult(dxr.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
      layer.mCanvasTexture.as<IDXGISurface>().get(),
      nullptr,
      layer.mCanvasBitmap.put()));

    winrt::check_hresult(dxr.mD3DDevice->CreateShaderResourceView(
      layer.mCanvasTexture.get(), nullptr, layer.mCanvasSRV.put()));
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
      auto textureName
        = SHM::SharedTextureName(sessionID, layerIndex, bufferIndex);
      winrt::check_hresult(
        resources.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
          nullptr,
          DXGI_SHARED_RESOURCE_READ,
          textureName.c_str(),
          resources.mSharedHandle.put()));
    }
  }

  AddEventListener(
    kneeboard->evNeedsRepaintEvent,
    std::bind_front(&InterprocessRenderer::MarkDirty, this));
  AddEventListener(
    kneeboard->evGameChangedEvent,
    std::bind_front(&InterprocessRenderer::OnGameChanged, this));

  const auto views = kneeboard->GetAllViewsInFixedOrder();
  for (int i = 0; i < views.size(); ++i) {
    auto view = views.at(i);
    mLayers.at(i).mKneeboardView = view;

    AddEventListener(
      view->evCursorEvent,
      std::bind_front(&InterprocessRenderer::MarkDirty, this));
  }

  AddEventListener(kneeboard->evFrameTimerEvent, [this]() {
    if (mNeedsRepaint) {
      RenderNow();
    }
  });
}

void InterprocessRenderer::MarkDirty() {
  mNeedsRepaint = true;
}

InterprocessRenderer::~InterprocessRenderer() {
  this->RemoveAllEventListeners();
  const std::unique_lock d2dLock(mDXR);
  auto ctx = mD3DContext;
  ctx->Flush();
}

void InterprocessRenderer::Render(RenderTargetID rtid, Layer& layer) {
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

  const auto vrc = mKneeboard->GetVRSettings();
  const auto xFitScale = vrc.mMaxWidth / usedSize.width;
  const auto yFitScale = vrc.mMaxHeight / usedSize.height;
  const auto scale = std::min<float>(xFitScale, yFitScale);

  layer.mConfig.mVR.mWidth = usedSize.width * scale;
  layer.mConfig.mVR.mHeight = usedSize.height * scale;

  view->RenderWithChrome(
    rtid,
    ctx.get(),
    {
      0,
      0,
      static_cast<FLOAT>(usedSize.width),
      static_cast<FLOAT>(usedSize.height),
    },
    layer.mIsActiveForInput);
}

void InterprocessRenderer::RenderNow() {
  const auto renderInfos = mKneeboard->GetViewRenderInfo();

  if (mRenderTargetIDs.size() < renderInfos.size()) {
    mRenderTargetIDs.resize(renderInfos.size());
  }

  for (uint8_t i = 0; i < renderInfos.size(); ++i) {
    auto& layer = mLayers.at(i);
    const auto& info = renderInfos.at(i);
    layer.mKneeboardView = info.mView;
    layer.mConfig.mVR = info.mVR;
    layer.mIsActiveForInput = info.mIsActiveForInput;
    this->Render(mRenderTargetIDs.at(i), layer);
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
