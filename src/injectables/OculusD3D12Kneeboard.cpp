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
  return &mSHM;
}

void OculusD3D12Kneeboard::UninstallHook() {
  mExecuteCommandListsHook.UninstallHook();
  mOculusKneeboard.UninstallHook();
}

ovrTextureSwapChain OculusD3D12Kneeboard::CreateSwapChain(
  ovrSession session,
  const PixelSize& size) {
  if (!mDevice) {
    return nullptr;
  }

  ovrTextureSwapChain swapChain = nullptr;

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

  auto ovr = OVRProxy::Get();

  ovr->ovr_CreateTextureSwapChainDX(
    session, mCommandQueue.get(), &kneeboardSCD, &swapChain);
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

  DirectX::DescriptorHeap rtvHeap {
    mDevice.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    static_cast<size_t>(length),
  };

  std::vector<SwapchainBufferResources> buffers;
  for (int i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D12Resource> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(texture.put()));
    buffers.push_back({
      mDevice.get(),
      texture.get(),
      rtvHeap.GetCpuHandle(i),
      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    });
  }

  mSwapchain = SwapchainResources {
    size,
    std::move(rtvHeap),
    std::move(buffers),
  };

  return swapChain;
}

void OculusD3D12Kneeboard::RenderLayers(
  ovrTextureSwapChain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  const PixelRect* const destRects,
  const float* const opacities) {
  OPENKNEEBOARD_TraceLoggingScope("OculusD3D12Kneeboard::RenderLayers()");

  mRenderer->RenderLayers(
    *mSwapchain,
    swapchainTextureIndex,
    snapshot,
    snapshot.GetLayerCount(),
    destRects,
    opacities,
    RenderMode::Overlay);
}

void OculusD3D12Kneeboard::OnID3D12CommandQueue_ExecuteCommandLists(
  ID3D12CommandQueue* this_,
  UINT NumCommandLists,
  ID3D12CommandList* const* ppCommandLists,
  const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next) {
  if (mSwapchain) {
    mExecuteCommandListsHook.UninstallHook();
    return std::invoke(next, this_, NumCommandLists, ppCommandLists);
  }

  auto commandQueue = this_;
  mCommandQueue.copy_from(commandQueue);

  winrt::com_ptr<ID3D12Device> device;
  winrt::check_hresult(this_->GetDevice(IID_PPV_ARGS(device.put())));
  if (device && commandQueue) {
    dprint("Got a D3D12 device and command queue");
  } else {
    OPENKNEEBOARD_BREAK;
  }

  mRenderer = std::make_unique<D3D12::Renderer>(
    device.get(), commandQueue, DXGI_FORMAT_B8G8R8A8_UNORM);

  auto cqDesc = commandQueue->GetDesc();
  if (cqDesc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT) {
    return std::invoke(next, this_, NumCommandLists, ppCommandLists);
  }

  mExecuteCommandListsHook.UninstallHook();
  return std::invoke(next, this_, NumCommandLists, ppCommandLists);
}

}// namespace OpenKneeboard
