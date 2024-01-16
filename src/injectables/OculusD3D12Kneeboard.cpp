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
#include "OculusD3D12Kneeboard.h"

#include "OVRProxy.h"

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <shims/winrt/base.h>

#include <d3d11_1.h>
#include <dxgi.h>

namespace OpenKneeboard {

OculusD3D12Kneeboard::OculusD3D12Kneeboard() {
  mExecuteCommandListsHook.InstallHook({
    .onExecuteCommandLists = std::bind_front(
      &OculusD3D12Kneeboard::OnID3D12CommandQueue_ExecuteCommandLists, this),
  });
  mOculusKneeboard.InstallHook(this);
}

OculusD3D12Kneeboard::~OculusD3D12Kneeboard() {
  UninstallHook();
}

SHM::CachedReader* OculusD3D12Kneeboard::GetSHM() {
  return mSHM.get();
}

void OculusD3D12Kneeboard::UninstallHook() {
  mExecuteCommandListsHook.UninstallHook();
  mOculusKneeboard.UninstallHook();
}

ovrTextureSwapChain OculusD3D12Kneeboard::CreateSwapChain(
  ovrSession session,
  uint8_t layerIndex) {
  auto dr = mDeviceResources.get();
  if (!dr) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  ovrTextureSwapChain swapChain = nullptr;

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

  auto ovr = OVRProxy::Get();

  ovr->ovr_CreateTextureSwapChainDX(
    session, dr->mD3D12CommandQueue.get(), &kneeboardSCD, &swapChain);
  if (!swapChain) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, swapChain, &length);
  if (length < 1) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  std::vector<ID3D12Resource*> textures;
  textures.reserve(length);
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D12Resource> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(texture.put()));
    textures.push_back(texture.get());
  }

  mSwapchainResources[swapChain] = std::make_unique<SwapchainResources>(
    dr, DXGI_FORMAT_B8G8R8A8_UNORM, textures.size(), textures.data());

  return swapChain;
}

bool OculusD3D12Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters& renderParameters) {
  auto ovr = OVRProxy::Get();

  int swapchainTextureIndex = -1;
  ovr->ovr_GetTextureSwapChainCurrentIndex(
    session, swapChain, &swapchainTextureIndex);
  if (swapchainTextureIndex < 0) {
    dprintf(" - invalid swap chain index ({})", swapchainTextureIndex);
    return false;
  }

  auto config = snapshot.GetLayerConfig(layerIndex);

  SHM::D3D12::Renderer::LayerSprite sprite {
    .mLayerIndex = layerIndex, .mDestRect = {
      0, 0,
      static_cast<LONG>(config->mImageWidth),
      static_cast<LONG>(config->mImageHeight),
    },
    .mOpacity = renderParameters.mKneeboardOpacity,
  };

  auto dr = mDeviceResources.get();
  auto sr = mSwapchainResources.at(swapChain).get();
  SHM::D3D12::Renderer::BeginFrame(dr, sr, swapchainTextureIndex);
  SHM::D3D12::Renderer::ClearRenderTargetView(dr, sr, swapchainTextureIndex);
  SHM::D3D12::Renderer::Render(
    *mSHM,
    snapshot,
    mDeviceResources.get(),
    mSwapchainResources.at(swapChain).get(),
    swapchainTextureIndex,
    1,
    &sprite);
  SHM::D3D12::Renderer::EndFrame(dr, sr, swapchainTextureIndex);

  auto error = ovr->ovr_CommitTextureSwapChain(session, swapChain);
  if (error) {
    dprintf("Commit failed with {}", error);
    return false;
  }

  return true;
}

void OculusD3D12Kneeboard::OnID3D12CommandQueue_ExecuteCommandLists(
  ID3D12CommandQueue* this_,
  UINT NumCommandLists,
  ID3D12CommandList* const* ppCommandLists,
  const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next) {
  if (mDeviceResources) {
    mExecuteCommandListsHook.UninstallHook();
    return std::invoke(next, this_, NumCommandLists, ppCommandLists);
  }

  auto commandQueue = this_;

  winrt::com_ptr<ID3D12Device> device;
  winrt::check_hresult(this_->GetDevice(IID_PPV_ARGS(device.put())));
  if (device && commandQueue) {
    dprint("Got a D3D12 device and command queue");
    if (!mSHM) {
      mSHM = std::make_unique<SHM::D3D12::CachedReader>(device.get());
    }
  } else {
    OPENKNEEBOARD_BREAK;
  }

  auto cqDesc = commandQueue->GetDesc();
  if (cqDesc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT) {
    return std::invoke(next, this_, NumCommandLists, ppCommandLists);
  }
  dprint("Got a D3D12_COMMAND_LIST_TYPE_DIRECT");
  mDeviceResources
    = std::make_unique<DeviceResources>(device.get(), commandQueue);

  mExecuteCommandListsHook.UninstallHook();
  return std::invoke(next, this_, NumCommandLists, ppCommandLists);
}

winrt::com_ptr<ID3D11Device> OculusD3D12Kneeboard::GetD3D11Device() {
  if (!mDeviceResources) {
    return {};
  }
  return mDeviceResources->mD3D11Device;
}

}// namespace OpenKneeboard
