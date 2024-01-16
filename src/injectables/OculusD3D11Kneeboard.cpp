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
#include "OculusD3D11Kneeboard.h"

#include "OVRProxy.h"

#include <OpenKneeboard/D3D11.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <OVR_CAPI_D3D.h>

namespace OpenKneeboard {

OculusD3D11Kneeboard::OculusD3D11Kneeboard() {
  dprintf("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
  mOculusKneeboard.InstallHook(this);
  mDXGIHook.InstallHook({
    .onPresent
    = std::bind_front(&OculusD3D11Kneeboard::OnIDXGISwapChain_Present, this),
  });
}

OculusD3D11Kneeboard::~OculusD3D11Kneeboard() {
  dprintf("{}, {:#018x}", __FUNCTION__, (uint64_t)this);
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
  if (!mDeviceResources) {
    return nullptr;
  }

  auto ovr = OVRProxy::Get();
  ovrTextureSwapChain swapChain = nullptr;

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  ovrTextureSwapChainDesc kneeboardSCD = {
    .Type = ovrTexture_2D,
    .Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB,
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
    session, mDeviceResources->mD3D11Device.get(), &kneeboardSCD, &swapChain);
  if (!swapChain) {
    dprint("ovr_CreateTextureSwapChainDX failed");
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, swapChain, &length);

  std::vector<winrt::com_ptr<ID3D11Texture2D>> textures;
  std::vector<ID3D11Texture2D*> texturePointers;
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D11Texture2D> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(texture.put()));
    textures.push_back(texture);
    texturePointers.push_back(texture.get());
  }

  mSwapchainResources[swapChain] = std::make_unique<SwapchainResources>(
    mDeviceResources.get(),
    DXGI_FORMAT_B8G8R8A8_UNORM,
    texturePointers.size(),
    texturePointers.data());

  return swapChain;
}

bool OculusD3D11Kneeboard::RenderLayers(
  ovrTextureSwapChain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  LayerRenderInfo* layers) {
  auto dr = mDeviceResources.get();
  auto sr = mSwapchainResources.at(swapchain).get();

  auto ctx = dr->mD3D11ImmediateContext;
  D3D11::SavedState state(ctx);

  namespace R = SHM::D3D11::Renderer;
  std::vector<R::LayerSprite> sprites;
  sprites.reserve(layerCount);
  for (uint8_t i = 0; i < layerCount; ++i) {
    const auto& layer = layers[i];
    sprites.push_back(R::LayerSprite {
      .mLayerIndex = layer.mLayerIndex,
      .mDestRect = layer.mDestRect,
      .mOpacity = layer.mVR.mKneeboardOpacity,
    });
  }

  R::BeginFrame(dr, sr, swapchainTextureIndex);
  R::ClearRenderTargetView(dr, sr, swapchainTextureIndex);
  R::Render(
    dr,
    sr,
    swapchainTextureIndex,
    mSHM,
    snapshot,
    sprites.size(),
    sprites.data());
  R::EndFrame(dr, sr, swapchainTextureIndex);

  return true;
}

HRESULT OculusD3D11Kneeboard::OnIDXGISwapChain_Present(
  IDXGISwapChain* swapChain,
  UINT syncInterval,
  UINT flags,
  const decltype(&IDXGISwapChain::Present)& next) {
  if (!mDeviceResources) {
    winrt::com_ptr<ID3D11Device> device;
    swapChain->GetDevice(IID_PPV_ARGS(device.put()));
    if (device) {
      mDeviceResources = std::make_unique<DeviceResources>(device.get());
    } else {
      dprint("Got a swapchain without a D3D11 device");
      OPENKNEEBOARD_BREAK;
    }
  }

  mDXGIHook.UninstallHook();
  return std::invoke(next, swapChain, syncInterval, flags);
}

winrt::com_ptr<ID3D11Device> OculusD3D11Kneeboard::GetD3D11Device() {
  if (!mDeviceResources) {
    return nullptr;
  }
  return mDeviceResources->mD3D11Device;
}

}// namespace OpenKneeboard
