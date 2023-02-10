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

#include <OVR_CAPI_D3D.h>
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include "OVRProxy.h"

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

ovrTextureSwapChain OculusD3D11Kneeboard::CreateSwapChain(
  ovrSession session,
  uint8_t layerIndex) {
  if (!mD3D) {
    return nullptr;
  }

  auto ovr = OVRProxy::Get();
  ovrTextureSwapChain swapChain = nullptr;

  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
  ovrTextureSwapChainDesc kneeboardSCD = {
    .Type = ovrTexture_2D,
    .Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB,
    .ArraySize = 1,
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .SampleCount = 1,
    .StaticImage = false,
    .MiscFlags = ovrTextureMisc_AutoGenerateMips,
    .BindFlags = ovrTextureBind_DX_RenderTarget,
  };

  ovr->ovr_CreateTextureSwapChainDX(
    session, mD3D.get(), &kneeboardSCD, &swapChain);
  if (!swapChain) {
    dprint("ovr_CreateTextureSwapChainDX failed");
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, swapChain, &length);

  auto& layerRenderTargets = mRenderTargetViews.at(layerIndex);
  layerRenderTargets.clear();
  layerRenderTargets.resize(length);
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D11Texture2D> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(texture.put()));

    layerRenderTargets.at(i) = std::make_shared<D3D11::RenderTargetViewFactory>(
      mD3D.get(), texture.get());
  }

  return swapChain;
}

bool OculusD3D11Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters& params) {
  return OculusD3D11Kneeboard::Render(
    mD3D.get(),
    mRenderTargetViews.at(layerIndex),
    session,
    swapChain,
    snapshot,
    layerIndex,
    params);
}

HRESULT OculusD3D11Kneeboard::OnIDXGISwapChain_Present(
  IDXGISwapChain* swapChain,
  UINT syncInterval,
  UINT flags,
  const decltype(&IDXGISwapChain::Present)& next) {
  if (!mD3D) {
    swapChain->GetDevice(IID_PPV_ARGS(mD3D.put()));
    if (!mD3D) {
      dprint("Got a swapchain without a D3D11 device");
    }
  }

  mDXGIHook.UninstallHook();
  return std::invoke(next, swapChain, syncInterval, flags);
}

winrt::com_ptr<ID3D11Device> OculusD3D11Kneeboard::GetD3D11Device() const {
  return mD3D;
}

}// namespace OpenKneeboard
