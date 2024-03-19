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

#include <OpenKneeboard/hresult.h>
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

  check_hresult(mDevice->CreateCommittedResource(
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
  ID3D12CommandAllocator* commandAllocator,
  ID3D12Resource* sourceTexture,
  ID3D12Fence* sourceFence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::CopyFrom");

  this->InitializeCacheTexture(sourceTexture);

  if (!mCommandLists.contains(sourceTexture)) {
    winrt::com_ptr<ID3D12GraphicsCommandList> list;
    check_hresult(mDevice->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      commandAllocator,
      nullptr,
      IID_PPV_ARGS(list.put())));
    mCommandLists.emplace(sourceTexture, list);
    this->PopulateCommandList(list.get(), sourceTexture);
  }

  const auto list = mCommandLists.at(sourceTexture).get();

  {
    OPENKNEEBOARD_TraceLoggingScope("SHM/D3D12/FenceIn");
    check_hresult(queue->Wait(sourceFence, fenceValueIn));
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("SHM/D3D12/ExecuteCommandLists");
    ID3D12CommandList* lists[] {list};
    queue->ExecuteCommandLists(std::size(lists), lists);
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("SHM/D3D12/FenceOut");
    check_hresult(queue->Signal(sourceFence, fenceValueOut));
  }
}

void Texture::PopulateCommandList(
  ID3D12GraphicsCommandList* list,
  ID3D12Resource* sourceTexture) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("PopulateCommandList");
  D3D12_RESOURCE_BARRIER inBarriers[] {
      D3D12_RESOURCE_BARRIER {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
          .pResource = this->mTexture.get(),
          .StateBefore = D3D12_RESOURCE_STATE_COMMON,
          .StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
        },
      },
    };
  list->ResourceBarrier(std::size(inBarriers), inBarriers);
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
  D3D12_RESOURCE_BARRIER outBarriers[] {
      D3D12_RESOURCE_BARRIER {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
          .pResource = this->mTexture.get(),
          .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
          .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        },
      },
    };
  check_hresult(list->Close());
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
      "SHM reader using adapter LUID {:#x}",
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

      check_hresult(mDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(br.mCommandAllocator.put())));
    }
  }

  SHM::CachedReader::InitializeCache(
    std::bit_cast<uint64_t>(device->GetAdapterLuid()), swapchainLength);
}

void CachedReader::ReleaseIPCHandles() {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::CachedReader::ReleaseIPCHandles");
  if (mIPCFences.empty()) {
    return;
  }
  std::vector<HANDLE> events;
  for (const auto& [fenceHandle, fenceAndValue]: mIPCFences) {
    const auto event = CreateEventEx(nullptr, nullptr, NULL, GENERIC_ALL);
    fenceAndValue.mFence->SetEventOnCompletion(fenceAndValue.mValue, event);
    events.push_back(event);
  }

  WaitForMultipleObjects(events.size(), events.data(), true, INFINITE);
  for (const auto event: events) {
    CloseHandle(event);
  }

  mIPCFences.clear();
  mIPCTextures.clear();
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

  const auto source = this->GetIPCTexture(sourceHandle);
  auto fenceAndValue = this->GetIPCFence(fenceHandle);

  reinterpret_cast<SHM::D3D12::Texture*>(destinationTexture)
    ->CopyFrom(
      mCommandQueue.get(),
      br.mCommandAllocator.get(),
      source,
      fenceAndValue->mFence.get(),
      fenceValueIn,
      fenceValueOut);

  fenceAndValue->mValue = fenceValueOut;
}

uint8_t CachedReader::GetSwapchainLength() const {
  return mBufferResources.size();
}

CachedReader::FenceAndValue* CachedReader::GetIPCFence(HANDLE handle) noexcept {
  if (mIPCFences.contains(handle)) {
    return &mIPCFences.at(handle);
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::GetIPCFence()");
  winrt::com_ptr<ID3D12Fence> fence;
  check_hresult(mDevice->OpenSharedHandle(handle, IID_PPV_ARGS(fence.put())));
  mIPCFences.emplace(handle, FenceAndValue {fence});
  return &mIPCFences.at(handle);
}

ID3D12Resource* CachedReader::GetIPCTexture(HANDLE handle) noexcept {
  if (mIPCTextures.contains(handle)) {
    return mIPCTextures.at(handle).get();
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::GetIPCTexture()");

  winrt::com_ptr<ID3D12Resource> texture;
  check_hresult(mDevice->OpenSharedHandle(handle, IID_PPV_ARGS(texture.put())));
  mIPCTextures.emplace(handle, texture);
  return texture.get();
}

}// namespace OpenKneeboard::SHM::D3D12