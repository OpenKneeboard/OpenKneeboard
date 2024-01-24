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
#include <OpenKneeboard/SHM/D3D12.h>

#include <OpenKneeboard/scope_guard.h>

#include <Windows.h>

#include <DirectXColors.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

namespace OpenKneeboard::SHM::D3D12 {

Texture::Texture(
  const winrt::com_ptr<ID3D12Device>& device,
  ID3D12DescriptorHeap* shaderResourceViewHeap,
  const D3D12_CPU_DESCRIPTOR_HANDLE& shaderResourceViewCPUHandle,
  const D3D12_GPU_DESCRIPTOR_HANDLE& shaderResourceViewGPUHandle)
  : mDevice(device),
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

void Texture::InitializeSource(
  HANDLE textureHandle,
  HANDLE fenceHandle) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::InitializeSource()");

  if (
    textureHandle == mSourceTextureHandle
    && fenceHandle == mSourceFenceHandle) {
    return;
  }

  if (textureHandle != mSourceTextureHandle) {
    mSourceTextureHandle = {};
    mSourceTexture = {};
    mTexture = {};
    mHaveShaderResourceView = false;
  }

  if (fenceHandle != mSourceFenceHandle) {
    mSourceFenceHandle = {};
    mSourceFence = {};
  }

  if (!mSourceTexture) {
    OPENKNEEBOARD_TraceLoggingScope("OpenTexture");
    winrt::check_hresult(mDevice->OpenSharedHandle(
      textureHandle, IID_PPV_ARGS(mSourceTexture.put())));

    const D3D12_HEAP_PROPERTIES heap {D3D12_HEAP_TYPE_DEFAULT};
    auto desc = mSourceTexture->GetDesc();
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

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
    mSourceTextureHandle = textureHandle;

    mTextureDimensions
      = {static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height)};
  }

  if (!mSourceFence) {
    OPENKNEEBOARD_TraceLoggingScope("OpenFence");
    winrt::check_hresult(
      mDevice->OpenSharedHandle(fenceHandle, IID_PPV_ARGS(mSourceFence.put())));
    mSourceFenceHandle = fenceHandle;
  }
}

PixelSize Texture::GetDimensions() const {
  return mTextureDimensions;
}

void Texture::CopyFrom(
  ID3D12CommandQueue* queue,
  ID3D12GraphicsCommandList* list,
  HANDLE texture,
  HANDLE fence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "SHM::D3D12::Texture::CopyFrom");

  this->InitializeSource(texture, fence);

  TraceLoggingWriteTagged(
    activity, "FenceIn", TraceLoggingValue(fenceValueIn, "Value"));
  winrt::check_hresult(queue->Wait(mSourceFence.get(), fenceValueIn));

  TraceLoggingWriteTagged(activity, "Copy");
  const D3D12_TEXTURE_COPY_LOCATION src {
    .pResource = this->mSourceTexture.get(),
    .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
    .SubresourceIndex = 0,
  };
  const D3D12_TEXTURE_COPY_LOCATION dst {
    .pResource = this->mTexture.get(),
    .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
    .SubresourceIndex = 0,
  };
  list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  TraceLoggingWriteTagged(activity, "CloseList");
  winrt::check_hresult(list->Close());

  TraceLoggingWriteTagged(activity, "ExecuteCommandLists");
  ID3D12CommandList* lists[] {list};
  queue->ExecuteCommandLists(std::size(lists), lists);

  TraceLoggingWriteTagged(
    activity, "FenceOut", TraceLoggingValue(fenceValueOut, "Value"));
  winrt::check_hresult(queue->Signal(mSourceFence.get(), fenceValueOut));
}

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::~CachedReader()");
}

void CachedReader::InitializeCache(
  ID3D12Device* device,
  ID3D12CommandQueue* queue,
  uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "swapchainLength"));

  if (device != mD3D12Device.get() || queue != mD3D12CommandQueue.get()) {
    mD3D12Device.copy_from(device);
    mD3D12CommandQueue.copy_from(queue);
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
      mD3D12Device.get(),
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      swapchainLength);

    mBufferResources.resize(swapchainLength);
    for (auto& br: mBufferResources) {
      if (br.mD3D12CommandAllocator) {
        continue;
      }

      winrt::check_hresult(mD3D12Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(br.mD3D12CommandAllocator.put())));
    }
  }

  SHM::CachedReader::InitializeCache(swapchainLength);
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  [[maybe_unused]] uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<SHM::D3D12::Texture>(
    mD3D12Device,
    mShaderResourceViewHeap->Heap(),
    mShaderResourceViewHeap->GetCpuHandle(swapchainIndex),
    mShaderResourceViewHeap->GetGpuHandle(swapchainIndex));
}

void CachedReader::Copy(
  uint8_t swapchainIndex,
  HANDLE sourceTexture,
  IPCClientTexture* destinationTexture,
  HANDLE fence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::CachedReader::Copy()");

  auto& br = mBufferResources.at(swapchainIndex);

  if (br.mD3D12CommandList) {
    winrt::check_hresult(
      br.mD3D12CommandList->Reset(br.mD3D12CommandAllocator.get(), nullptr));
  } else {
    winrt::check_hresult(mD3D12Device->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      br.mD3D12CommandAllocator.get(),
      nullptr,
      IID_PPV_ARGS(br.mD3D12CommandList.put())));
  }

  reinterpret_cast<SHM::D3D12::Texture*>(destinationTexture)
    ->CopyFrom(
      mD3D12CommandQueue.get(),
      br.mD3D12CommandList.get(),
      sourceTexture,
      fence,
      fenceValueIn,
      fenceValueOut);
}

uint8_t CachedReader::GetSwapchainLength() const {
  return mBufferResources.size();
}

}// namespace OpenKneeboard::SHM::D3D12