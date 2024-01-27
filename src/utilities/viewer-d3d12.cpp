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

#include "viewer-d3d12.h"

#include <OpenKneeboard/D3D12.h>
#include <OpenKneeboard/RenderDoc.h>

#include <directxtk12/ScreenGrab.h>

namespace OpenKneeboard::Viewer {

D3D12Renderer::D3D12Renderer(IDXGIAdapter* dxgiAdapter) {
  dprint(__FUNCSIG__);

#ifdef DEBUG
  dprint("Enabling D3D12 debug features");
  winrt::com_ptr<ID3D12Debug5> debug;
  winrt::check_hresult(D3D12GetDebugInterface(IID_PPV_ARGS(debug.put())));
  debug->EnableDebugLayer();
#endif

  mDevice.capture(D3D12CreateDevice, dxgiAdapter, D3D_FEATURE_LEVEL_12_1);

#ifdef DEBUG
  if (!RenderDoc::IsPresent()) {
    // There's also the semi-documented ID3D12InfoQueue1 interface if we end up
    // wanting a full callback
    const auto infoQueue = mDevice.try_as<ID3D12InfoQueue>();
    if (infoQueue) {
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    }
  }
#endif

  D3D12_COMMAND_QUEUE_DESC queueDesc {D3D12_COMMAND_LIST_TYPE_DIRECT};
  mCommandQueue.capture(mDevice, &ID3D12Device::CreateCommandQueue, &queueDesc);
  mCommandAllocator.capture(
    mDevice,
    &ID3D12Device::CreateCommandAllocator,
    D3D12_COMMAND_LIST_TYPE_DIRECT);

  mSpriteBatch = std::make_unique<D3D12::SpriteBatch>(
    mDevice.get(), mCommandQueue.get(), DXGI_FORMAT_B8G8R8A8_UNORM);

  mDestRTVHeap = std::make_unique<DirectX::DescriptorHeap>(
    mDevice.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    1);
}

D3D12Renderer::~D3D12Renderer() = default;

std::wstring_view D3D12Renderer::GetName() const noexcept {
  return {L"D3D12"};
}

SHM::CachedReader* D3D12Renderer::GetSHM() {
  return &mSHM;
}

void D3D12Renderer::Initialize(uint8_t swapchainLength) {
  mSHM.InitializeCache(mDevice.get(), mCommandQueue.get(), swapchainLength);
}

uint64_t D3D12Renderer::Render(
  SHM::IPCClientTexture* sourceTexture,
  const PixelRect& sourceRect,
  HANDLE destTextureHandle,
  const PixelSize& destTextureDimensions,
  const PixelRect& destRect,
  HANDLE fenceHandle,
  uint64_t fenceValueIn) {
  OPENKNEEBOARD_TraceLoggingScope("Viewer::D3D12Renderer::Render");

  RenderDoc::NestedFrameCapture renderDocFrame(
    mDevice.get(), "D3D12Renderer::Render()");

  if (mDestDimensions != destTextureDimensions) {
    mDestHandle = {};
  }
  if (destTextureHandle != mDestHandle) {
    mDestTexture = nullptr;
    mDestTexture.capture(
      mDevice, &ID3D12Device::OpenSharedHandle, destTextureHandle);
    mDestHandle = destTextureHandle;
    mDestDimensions = destTextureDimensions;
  }

  if (fenceHandle != mFenceHandle) {
    mFence.capture(mDevice, &ID3D12Device::OpenSharedHandle, fenceHandle);
    mFenceHandle = fenceHandle;
  }

  auto source = reinterpret_cast<SHM::D3D12::Texture*>(sourceTexture);
  auto dest = mDestRTVHeap->GetFirstCpuHandle();
  mDevice->CreateRenderTargetView(mDestTexture.get(), nullptr, dest);

  if (!mCommandList) {
    mCommandList.capture(
      mDevice,
      &ID3D12Device::CreateCommandList,
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      mCommandAllocator.get(),
      nullptr);
  }

  auto cl = mCommandList.get();

  ID3D12DescriptorHeap* heaps[] {source->GetD3D12ShaderResourceViewHeap()};
  cl->SetDescriptorHeaps(std::size(heaps), heaps);

  D3D12_RESOURCE_BARRIER inBarriers[] {
    D3D12_RESOURCE_BARRIER {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
        .pResource = mDestTexture.get(),
        .StateBefore = D3D12_RESOURCE_STATE_COMMON,
        .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
      },
    },
  };
  cl->ResourceBarrier(std::size(inBarriers), inBarriers);

  mSpriteBatch->Begin(cl, dest, destTextureDimensions);
  mSpriteBatch->Draw(
    source->GetD3D12ShaderResourceViewGPUHandle(),
    source->GetDimensions(),
    sourceRect,
    destRect);
  mSpriteBatch->End();

  D3D12_RESOURCE_BARRIER outBarriers[] {
    D3D12_RESOURCE_BARRIER {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
        .pResource = mDestTexture.get(),
        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
        .StateAfter = D3D12_RESOURCE_STATE_COMMON,
      },
    },
  };
  cl->ResourceBarrier(std::size(outBarriers), outBarriers);

  winrt::check_hresult(cl->Close());

  winrt::check_hresult(mCommandQueue->Wait(mFence.get(), fenceValueIn));

  ID3D12CommandList* lists[] {cl};
  mCommandQueue->ExecuteCommandLists(std::size(lists), lists);

  const auto fenceValueOut = fenceValueIn + 1;
  winrt::check_hresult(mCommandQueue->Signal(mFence.get(), fenceValueOut));

  {
    OPENKNEEBOARD_TraceLoggingScope("ResetCommandList");
    winrt::check_hresult(cl->Reset(mCommandAllocator.get(), nullptr));
  }

  return fenceValueOut;
}

void D3D12Renderer::SaveTextureToFile(
  SHM::IPCClientTexture* texture,
  const std::filesystem::path& path) {
  SaveTextureToFile(
    reinterpret_cast<SHM::D3D12::Texture*>(texture)->GetD3D12Texture(), path);
}
void D3D12Renderer::SaveTextureToFile(
  ID3D12Resource* texture,
  const std::filesystem::path& path) {
  winrt::check_hresult(DirectX::SaveDDSTextureToFile(
    mCommandQueue.get(), texture, path.wstring().c_str()));
}

}// namespace OpenKneeboard::Viewer