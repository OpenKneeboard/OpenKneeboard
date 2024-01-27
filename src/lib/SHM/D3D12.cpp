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
#include <OpenKneeboard/D3D12.h>
#include <OpenKneeboard/RenderDoc.h>
#include <OpenKneeboard/SHM/D3D12.h>

#include <OpenKneeboard/scope_guard.h>

#include <Windows.h>

#include <DirectXColors.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

namespace OpenKneeboard::SHM::D3D12 {

Texture::Texture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex,
  const winrt::com_ptr<ID3D12Device>& device,
  ID3D12DescriptorHeap* shaderResourceViewHeap,
  const D3D12_CPU_DESCRIPTOR_HANDLE& shaderResourceViewCPUHandle,
  const D3D12_GPU_DESCRIPTOR_HANDLE& shaderResourceViewGPUHandle)
  : IPCClientTexture(dimensions, swapchainIndex),
    mDevice(device),
    mShaderResourceViewCPUHandle(shaderResourceViewCPUHandle),
    mShaderResourceViewGPUHandle(shaderResourceViewGPUHandle) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::Texture()");

  mShaderResourceViewHeap.copy_from(shaderResourceViewHeap);
}

Texture::~Texture() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::~Texture()");
}

ID3D12Resource* Texture::GetD3D12Texture() {
  return mTexture.get();
}

ID3D12DescriptorHeap* Texture::GetD3D12ShaderResourceViewHeap() {
  return mShaderResourceViewHeap.get();
}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetD3D12ShaderResourceViewGPUHandle() {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::Texture::GetD3D12ShaderResourceViewGPUHandle()");
  if (mHaveShaderResourceView) {
    return mShaderResourceViewGPUHandle;
  }

  if (!mTexture) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Can't create an SRV without a texture");
  }

  mDevice->CreateShaderResourceView(
    mTexture.get(), nullptr, mShaderResourceViewCPUHandle);
  mHaveShaderResourceView = true;

  return mShaderResourceViewGPUHandle;
}

void Texture::InitializeCacheTexture(ID3D12Resource* sourceTexture) noexcept {
  if (mTexture) {
    return;
  }
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::Texture::InitializeCacheTexture()");

  const D3D12_HEAP_PROPERTIES heap {D3D12_HEAP_TYPE_DEFAULT};
  auto desc = sourceTexture->GetDesc();
  desc.Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT;

  const D3D12_CLEAR_VALUE clearValue {
    .Format = desc.Format,
    .Color = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  winrt::check_hresult(mDevice->CreateCommittedResource(
    &heap,
    D3D12_HEAP_FLAG_NONE,
    &desc,
    D3D12_RESOURCE_STATE_COMMON,
    &clearValue,
    IID_PPV_ARGS(mTexture.put())));

  mTextureDimensions
    = {static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height)};
}

void Texture::CopyFrom(
  ID3D12CommandQueue* queue,
  ID3D12GraphicsCommandList* list,
  ID3D12Resource* sourceTexture,
  ID3D12Fence* sourceFence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::CopyFrom");

  this->InitializeCacheTexture(sourceTexture);

  {
    OPENKNEEBOARD_TraceLoggingScope("FenceIn");
    winrt::check_hresult(queue->Wait(sourceFence, fenceValueIn));
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("PopulateCommandList");
    const D3D12_TEXTURE_COPY_LOCATION src {
      .pResource = sourceTexture,
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
    };
    const D3D12_TEXTURE_COPY_LOCATION dst {
      .pResource = this->mTexture.get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
    };
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    winrt::check_hresult(list->Close());
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("ExecuteCommandLists");
    ID3D12CommandList* lists[] {list};
    queue->ExecuteCommandLists(std::size(lists), lists);
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("FenceOut");
    winrt::check_hresult(queue->Signal(sourceFence, fenceValueOut));
  }
}

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::~CachedReader()");
}

Snapshot CachedReader::MaybeGet(const std::source_location& loc) {
  RenderDoc::NestedFrameCapture renderDocFrame(
    mDevice.get(), "SHM::D3D12::MaybeGet");
  return SHM::CachedReader::MaybeGet(loc);
}

void CachedReader::InitializeCache(
  ID3D12Device* device,
  ID3D12CommandQueue* queue,
  uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "swapchainLength"));

  if (device != mDevice.get() || queue != mCommandQueue.get()) {
    mDevice.copy_from(device);
    mCommandQueue.copy_from(queue);
    mBufferResources = {};

    // debug logging
    // It's a pain to get the adapater name with D3D12; you 'should'
    // go via IDXGIFactory, however an app "shouldn't" use both IDXGIFactory
    // and IDXGIFactory1 (or later) in the same process, and we don't know
    // which the app picked
    dprintf(
      L"SHM reader using adapter LUID {:#x}",
      std::bit_cast<uint64_t>(device->GetAdapterLuid()));
  };

  if (swapchainLength != GetSwapchainLength()) {
    mShaderResourceViewHeap = std::make_unique<DirectX::DescriptorHeap>(
      mDevice.get(),
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      swapchainLength);

    mBufferResources.resize(swapchainLength);
    for (auto& br: mBufferResources) {
      if (br.mCommandAllocator) {
        continue;
      }

      winrt::check_hresult(mDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(br.mCommandAllocator.put())));
    }
  }

  SHM::CachedReader::InitializeCache(swapchainLength);
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<SHM::D3D12::Texture>(
    dimensions,
    swapchainIndex,
    mDevice,
    mShaderResourceViewHeap->Heap(),
    mShaderResourceViewHeap->GetCpuHandle(swapchainIndex),
    mShaderResourceViewHeap->GetGpuHandle(swapchainIndex));
}

void CachedReader::Copy(
  HANDLE sourceHandle,
  IPCClientTexture* destinationTexture,
  HANDLE fenceHandle,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::Copy()");

  const auto swapchainIndex = destinationTexture->GetSwapchainIndex();

  auto& br = mBufferResources.at(swapchainIndex);

  if (!br.mCommandList) {
    OPENKNEEBOARD_TraceLoggingScope("CreateCommandList");
    winrt::check_hresult(mDevice->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      br.mCommandAllocator.get(),
      nullptr,
      IID_PPV_ARGS(br.mCommandList.put())));
  }

  const auto source = this->GetIPCTexture(sourceHandle);
  const auto fence = this->GetIPCFence(fenceHandle);

  reinterpret_cast<SHM::D3D12::Texture*>(destinationTexture)
    ->CopyFrom(
      mCommandQueue.get(),
      br.mCommandList.get(),
      source,
      fence,
      fenceValueIn,
      fenceValueOut);

  OPENKNEEBOARD_TraceLoggingScope("ResetCommandList");
  winrt::check_hresult(
    br.mCommandList->Reset(br.mCommandAllocator.get(), nullptr));
}

uint8_t CachedReader::GetSwapchainLength() const {
  return mBufferResources.size();
}

ID3D12Fence* CachedReader::GetIPCFence(HANDLE handle) noexcept {
  if (mIPCFences.contains(handle)) {
    return mIPCFences.at(handle).get();
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::GetIPCFence()");
  winrt::com_ptr<ID3D12Fence> fence;
  winrt::check_hresult(
    mDevice->OpenSharedHandle(handle, IID_PPV_ARGS(fence.put())));
  mIPCFences.emplace(handle, fence);
  return fence.get();
}

ID3D12Resource* CachedReader::GetIPCTexture(HANDLE handle) noexcept {
  if (mIPCTextures.contains(handle)) {
    return mIPCTextures.at(handle).get();
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::GetIPCTexture()");

  winrt::com_ptr<ID3D12Resource> texture;
  winrt::check_hresult(
    mDevice->OpenSharedHandle(handle, IID_PPV_ARGS(texture.put())));
  mIPCTextures.emplace(handle, texture);
  return texture.get();
}

}// namespace OpenKneeboard::SHM::D3D12