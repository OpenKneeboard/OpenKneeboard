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

#include <DirectXTK12/ResourceUploadBatch.h>
#include <OpenKneeboard/dprint.h>

#include "InjectedDLLMain.h"
#include "OVRProxy.h"

#include <winrt/base.h>

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
  const SHM::Config& config) {
  if (!(mCommandQueue && mDevice)) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  ovrTextureSwapChain swapChain = nullptr;

  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
  ovrTextureSwapChainDesc kneeboardSCD = {
    .Type = ovrTexture_2D,
    .Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB,
    .ArraySize = 1,
    .Width = config.imageWidth,
    .Height = config.imageHeight,
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

  mDescriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC dhDesc {
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    .NumDescriptors = static_cast<UINT>(length),
    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    .NodeMask = 0,
  };
  mDevice->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(mDescriptorHeap.put()));
  if (!mDescriptorHeap) {
    dprintf("Failed to get descriptor heap: {:#x}", GetLastError());
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  auto heap = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  auto increment = mDevice->GetDescriptorHandleIncrementSize(dhDesc.Type);

  for (auto i = 0; i < length; ++i) {
    winrt::com_ptr<ID3D12Resource> texture;
    ovr->ovr_GetTextureSwapChainBufferDX(
      session, swapChain, i, IID_PPV_ARGS(texture.put()));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {},
    };

    auto dest = heap;
    dest.ptr += (i * increment);
    mDevice->CreateRenderTargetView(texture.get(), &rtvDesc, dest);
  }

  return swapChain;
}

bool OculusD3D12Kneeboard::Render(
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot) {
  auto ovr = OVRProxy::Get();
  const auto& config = *snapshot.GetConfig();

  int index = -1;
  ovr->ovr_GetTextureSwapChainCurrentIndex(session, swapChain, &index);
  if (index < 0) {
    dprintf(" - invalid swap chain index ({})", index);
    OPENKNEEBOARD_BREAK;
    return false;
  }

  winrt::com_ptr<ID3D12Resource> layerTexture;
  ovr->ovr_GetTextureSwapChainBufferDX(
    session, swapChain, index, IID_PPV_ARGS(layerTexture.put()));
  if (!layerTexture) {
    OPENKNEEBOARD_BREAK;
    return false;
  }

  auto sharedTextureName = SHM::SharedTextureName();
  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
  HANDLE sharedHandle = INVALID_HANDLE_VALUE;
  auto oshbnRet = mDevice->OpenSharedHandleByName(
    sharedTextureName.c_str(), GENERIC_ALL, &sharedHandle);
  if (sharedHandle == INVALID_HANDLE_VALUE || !sharedHandle) {
    return false;
  }

  winrt::com_ptr<ID3D12Resource> sharedTexture;
  auto oshRet = mDevice->OpenSharedHandle(
    sharedHandle, IID_PPV_ARGS(sharedTexture.put()));
  if (!sharedTexture) {
    return false;
  }

  D3D12_TEXTURE_COPY_LOCATION dest {
    .pResource = layerTexture.get(),
    .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
    .SubresourceIndex = 0,
  };
  D3D12_TEXTURE_COPY_LOCATION src {
    .pResource = sharedTexture.get(),
    .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
    .SubresourceIndex = 0,
  };
  D3D12_BOX srcBox {0, 0, 0, config.imageWidth, config.imageHeight, 1};

  // Recreate every time as it becomes read-only
  winrt::com_ptr<ID3D12GraphicsCommandList1> commandList;
  mDevice->CreateCommandList(
    1,
    mCommandQueue->GetDesc().Type,
    mCommandAllocator.get(),
    nullptr,
    IID_PPV_ARGS(commandList.put()));
  commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, &srcBox);
  commandList->Close();
  auto list = static_cast<ID3D12CommandList*>(commandList.get());

  winrt::handle event { CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE) };
  winrt::com_ptr<ID3D12Fence> fence;
  mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put()));

  mCommandQueue->ExecuteCommandLists(1, &list);
  mCommandQueue->Signal(fence.get(), 1);
  fence->SetEventOnCompletion(1, event.get());

  WaitForSingleObject(event.get(), INFINITE);

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

    mDevice->CreateCommandAllocator(
      cqDesc.Type, IID_PPV_ARGS(mCommandAllocator.put()));
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
    FMT_STRING("----- OculusD3D12Kneeboard active at {:#018x} -----"),
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
