// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include "OculusD3D11Kneeboard.hpp"

#include "OVRProxy.hpp"

#include <OpenKneeboard/D3D11.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>

#include <OVR_CAPI_D3D.h>

namespace OpenKneeboard {

OculusD3D11Kneeboard::OculusD3D11Kneeboard() {
  dprint("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
  mOculusKneeboard.InstallHook(this);
  mDXGIHook.InstallHook({
    .onPresent
    = std::bind_front(&OculusD3D11Kneeboard::OnIDXGISwapChain_Present, this),
  });
}

OculusD3D11Kneeboard::~OculusD3D11Kneeboard() {
  dprint("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
  this->UninstallHook();
}

void OculusD3D11Kneeboard::UninstallHook() {
  mOculusKneeboard.UninstallHook();
  mDXGIHook.UninstallHook();
}

SHM::CachedReader* OculusD3D11Kneeboard::GetSHM() {
  return &mSHM;
}

ovrTextureSwapChain OculusD3D11Kneeboard::CreateSwapChain(
  ovrSession session,
  const PixelSize& size) {
  if (!mD3D11Device) {
    TraceLoggingWrite(
      gTraceProvider, "OculusD3D11Kneeboard::CreateSwapChain()/NoD3D11Device");
    return nullptr;
  }

  auto ovr = OVRProxy::Get();
  ovrTextureSwapChain swapChain = nullptr;

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  ovrTextureSwapChainDesc kneeboardSCD = {
    .Type = ovrTexture_2D,
    .Format = OVR_FORMAT_B8G8R8A8_UNORM,
    .ArraySize = 1,
    .Width = static_cast<int>(size.mWidth),
    .Height = static_cast<int>(size.mHeight),
    .MipLevels = 1,
    .SampleCount = 1,
    .StaticImage = false,
    .MiscFlags = ovrTextureMisc_AutoGenerateMips,
    .BindFlags = ovrTextureBind_DX_RenderTarget,
  };

  ovr->ovr_CreateTextureSwapChainDX(
    session, mD3D11Device.get(), &kneeboardSCD, &swapChain);
  if (!swapChain) {
    dprint("ovr_CreateTextureSwapChainDX failed");
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, swapChain, &length);
  if (length < 1) {
    dprint("Got an invalid swapchain length of {}", length);
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  std::vector<SwapchainBufferResources> buffers;
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D11Texture2D> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(texture.put()));
    buffers.push_back(
      {mD3D11Device.get(), texture.get(), DXGI_FORMAT_B8G8R8A8_UNORM});
  }
  mSwapchain = {size, std::move(buffers)};

  mSHM.InitializeCache(mD3D11Device.get(), static_cast<uint8_t>(length));

  return swapChain;
}

void OculusD3D11Kneeboard::RenderLayers(
  ovrTextureSwapChain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  const std::span<SHM::LayerSprite>& layers) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "OculusD3D11::RenderLayers");

  D3D11::ScopedDeviceContextStateChange savedState {
    mD3D11DeviceContext, &mRenderState};

  mRenderer->RenderLayers(
    *mSwapchain,
    swapchainTextureIndex,
    snapshot,
    layers,
    RenderMode::ClearAndRender);
}

HRESULT OculusD3D11Kneeboard::OnIDXGISwapChain_Present(
  IDXGISwapChain* swapChain,
  UINT syncInterval,
  UINT flags,
  const decltype(&IDXGISwapChain::Present)& next) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "OculusD3D11Kneeboard::OnIDXGISwapChain_Present()");
  if (!mD3D11Device) {
    OPENKNEEBOARD_TraceLoggingScope("InitResources");
    winrt::check_hresult(
      swapChain->GetDevice(IID_PPV_ARGS(mD3D11Device.put())));
    winrt::com_ptr<ID3D11DeviceContext> ctx;
    mD3D11Device->GetImmediateContext(ctx.put());
    mD3D11DeviceContext = ctx.as<ID3D11DeviceContext1>();
    mRenderer = std::make_unique<D3D11::Renderer>(mD3D11Device.get());
  }

  mDXGIHook.UninstallHook();
  return std::invoke(next, swapChain, syncInterval, flags);
}

}// namespace OpenKneeboard
