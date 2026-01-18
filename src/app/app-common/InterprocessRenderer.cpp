// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/CreateTabActions.hpp>
#include <OpenKneeboard/CursorClickableRegions.hpp>
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/CursorRenderer.hpp>
#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/GetSystemColor.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/InterprocessRenderer.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/Spriting.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <mutex>
#include <ranges>

#include <DirectXColors.h>
#include <d3d11_4.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

namespace OpenKneeboard {

void InterprocessRenderer::SubmitFrame(
  const std::vector<SHM::LayerConfig>& shmLayers,
  uint64_t /*inputLayerID*/) noexcept {
  if (!mSHM) {
    return;
  }

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "InterprocessRenderer::SubmitFrame()");

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
      TraceLoggingValue(ipcTextureInfo.mFenceOut, "FenceOut"));
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
    .mGlobalInputLayerID
    = mKneeboard->GetActiveInGameView()->GetRuntimeID().GetTemporaryValue(),
    .mVR = static_cast<const VRRenderSettings&>(mKneeboard->GetVRSettings()),
    .mTextureSize = destResources->mTextureSize,
  };
  const auto tint = mKneeboard->GetUISettings().mTint;
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
      ipcTextureInfo,
      config,
      shmLayers,
      destResources->mTextureHandle.get(),
      destResources->mFenceHandle.get());
  }
}

uint64_t InterprocessRenderer::GetFrameCountForMetricsOnly() const {
  return mSHM.GetFrameCountForMetricsOnly();
}

void InterprocessRenderer::InitializeCanvas(const PixelSize& size) {
  if (mCanvasSize == size) {
    return;
  }

  OPENKNEEBOARD_TraceLoggingScope("InterprocessRenderer::InitializeCanvas()");

  if (size.IsEmpty()) [[unlikely]] {
    OPENKNEEBOARD_BREAK;
    return;
  }

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

  // Force an SHM update, even if we don't have new pixels
  if (!mVisible) {
    mPreviousFrameWasVisible = true;
  }

  mKneeboard->SetRepaintNeeded();
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
  // Our IPC textures will be used within SHM::SwapchainLength (3) frames, so
  // evicting them from VRAM to RAM will pretty much always cause problems.
  ret.mTexture->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

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

OpenKneeboard::fire_and_forget InterprocessRenderer::final_release(
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
  mKneeboard = kneeboard;
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

task<SHM::LayerConfig> InterprocessRenderer::RenderLayer(
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

  co_await view->RenderWithChrome(
    mCanvas.get(),
    PixelRect {bounds.mOffset, layer.mFullSize},
    layer.mIsActiveForInput);

  co_return ret;
}

task<void> InterprocessRenderer::RenderNow() noexcept {
  if (mRendering.test_and_set()) {
    dprint("Two renders in the same instance");
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  const scope_exit markDone([this]() { mRendering.clear(); });

  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "InterprocessRenderer::RenderNow()");

  const auto renderInfos = mKneeboard->GetViewRenderInfo();
  const auto layerCount = renderInfos.size();

  // layerCount == 0 'should' be impossible as it's not meant to be possible to
  // disable the non-VR view for view 1, however a bug in v1.10.0 and v1.10.2
  // could lead to view 1 being fully disabled
  if (layerCount == 0 || !mVisible) {
    if (layerCount == 0) {
      TraceLoggingWriteTagged(activity, "NoLayers");
    } else {
      TraceLoggingWriteTagged(activity, "Invisible");
    }
    if (mSHM && mPreviousFrameWasVisible) {
      std::unique_lock lock(mSHM);
      mSHM.SubmitEmptyFrame();
    }
    mPreviousFrameWasVisible = false;
    co_return;
  }
  mPreviousFrameWasVisible = true;

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

    shmLayers.push_back(co_await this->RenderLayer(renderInfo, bounds));
  }

  this->SubmitFrame(shmLayers, inputLayerID);
}

}// namespace OpenKneeboard
