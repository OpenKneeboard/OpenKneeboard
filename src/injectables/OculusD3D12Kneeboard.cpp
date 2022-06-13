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

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <shims/winrt.h>

#include "InjectedDLLMain.h"
#include "OVRProxy.h"

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

void OculusD3D12Kneeboard::UninstallHook() {
  mExecuteCommandListsHook.UninstallHook();
  mOculusKneeboard.UninstallHook();
}

ovrTextureSwapChain OculusD3D12Kneeboard::CreateSwapChain(
  ovrSession session,
  uint8_t layerIndex) {
  if (!(mCommandQueue && mDevice)) {
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
    session, mCommandQueue.get(), &kneeboardSCD, &swapChain);
  if (!swapChain) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  int length = -1;
  ovr->ovr_GetTextureSwapChainLength(session, swapChain, &length);
  if (length == -1) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  return swapChain;
}

bool OculusD3D12Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters&) {
  const auto& layer = *snapshot.GetLayerConfig(layerIndex);
  if (!layer.IsValid()) {
    return false;
  }

  auto ovr = OVRProxy::Get();

  int index = -1;
  ovr->ovr_GetTextureSwapChainCurrentIndex(session, swapChain, &index);
  if (index < 0) {
    dprintf(" - invalid swap chain index ({})", index);
    OPENKNEEBOARD_BREAK;
    return false;
  }

  winrt::com_ptr<ID3D12Resource> layerTexture12;
  ovr->ovr_GetTextureSwapChainBufferDX(
    session, swapChain, index, IID_PPV_ARGS(layerTexture12.put()));
  if (!layerTexture12) {
    OPENKNEEBOARD_BREAK;
    return false;
  }

  auto sharedTexture = snapshot.GetLayerTexture(m11on12.get(), layerIndex);
  if (!sharedTexture.IsValid()) {
    return false;
  }
  auto sharedTexture11 = sharedTexture.GetTexture();

  winrt::com_ptr<ID3D11Texture2D> layerTexture11;
  D3D11_RESOURCE_FLAGS resourceFlags11 {};
  m11on12.as<ID3D11On12Device2>()->CreateWrappedResource(
    layerTexture12.get(),
    &resourceFlags11,
    D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_STATE_COMMON,
    IID_PPV_ARGS(layerTexture11.put()));

  D3D11_BOX sourceBox {
    .left = 0,
    .top = 0,
    .front = 0,
    .right = layer.mImageWidth,
    .bottom = layer.mImageHeight,
    .back = 1,
  };

  m11on12Context->CopySubresourceRegion(
    layerTexture11.get(), 0, 0, 0, 0, sharedTexture11, 0, &sourceBox);
  m11on12Context->Flush();

  ovr->ovr_CommitTextureSwapChain(session, swapChain);

  return true;
}

void OculusD3D12Kneeboard::OnID3D12CommandQueue_ExecuteCommandLists(
  ID3D12CommandQueue* this_,
  UINT NumCommandLists,
  ID3D12CommandList* const* ppCommandLists,
  const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next) {
  if (!mCommandQueue) {
    mCommandQueue.attach(this_);
    mCommandQueue->AddRef();
    this_->GetDevice(IID_PPV_ARGS(mDevice.put()));
    if (mCommandQueue && mDevice) {
      dprint("Got a D3D12 device and command queue");
    } else {
      OPENKNEEBOARD_BREAK;
    }

    auto cqDesc = mCommandQueue->GetDesc();

    switch (cqDesc.Type) {
      case D3D12_COMMAND_LIST_TYPE_COMPUTE:
      case D3D12_COMMAND_LIST_TYPE_COPY:
      case D3D12_COMMAND_LIST_TYPE_DIRECT:
        // Command list supports COPY. All we need is COPY, but our command list
        // must use the same type as the command queue
        break;
      default:
        throw std::logic_error("Unsupported command queue type");
    }

    // We need this as D3D12 does not appear to directly support IDXGIKeyedMutex
    UINT flags = 0;
#ifdef DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D11On12CreateDevice(
      mDevice.get(),
      flags,
      nullptr,
      0,
      nullptr,
      0,
      1,
      m11on12.put(),
      m11on12Context.put(),
      nullptr);
  }

  mExecuteCommandListsHook.UninstallHook();
  return std::invoke(next, this_, NumCommandLists, ppCommandLists);
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {
std::unique_ptr<OculusD3D12Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<OculusD3D12Kneeboard>();
  dprintf(
    "----- OculusD3D12Kneeboard active at {:#018x} -----",
    (intptr_t)gInstance.get());
  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-Oculus-D3D12",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
