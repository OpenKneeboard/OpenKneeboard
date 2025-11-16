// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/SHM.hpp>

#include <shims/winrt/base.h>

#include <d3d12.h>

#include <directxtk12/DescriptorHeap.h>

namespace OpenKneeboard::SHM::D3D12 {

class Texture final : public SHM::IPCClientTexture {
 public:
  Texture() = delete;
  Texture(
    const PixelSize&,
    uint8_t swapchainIndex,
    const winrt::com_ptr<ID3D12Device>&,
    const winrt::com_ptr<ID3D12CommandQueue>&,
    const winrt::com_ptr<ID3D12Fence>& fenceOut,
    ID3D12DescriptorHeap* shaderResourceViewHeap,
    const D3D12_CPU_DESCRIPTOR_HANDLE& shaderResourceViewCPUHandle,
    const D3D12_GPU_DESCRIPTOR_HANDLE& shaderResourceViewGPUHandle);
  virtual ~Texture();

  ID3D12Resource* GetD3D12Texture();
  D3D12_GPU_DESCRIPTOR_HANDLE GetD3D12ShaderResourceViewGPUHandle();
  ID3D12DescriptorHeap* GetD3D12ShaderResourceViewHeap();

  void ReleaseCommandLists();

  void CopyFrom(
    ID3D12CommandAllocator*,
    ID3D12Resource* texture,
    ID3D12Fence* fenceIn,
    uint64_t fenceInValue,
    uint64_t fenceOutValue) noexcept;

 private:
  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;
  winrt::com_ptr<ID3D12Fence> mFenceOut;
  uint64_t mFenceOutValue {};

  winrt::com_ptr<ID3D12DescriptorHeap> mShaderResourceViewHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE mShaderResourceViewCPUHandle;
  D3D12_GPU_DESCRIPTOR_HANDLE mShaderResourceViewGPUHandle;

  void PopulateCommandList(
    ID3D12GraphicsCommandList*,
    ID3D12Resource* texture) noexcept;

  winrt::com_ptr<ID3D12Resource> mTexture;

  // Must be deleted before the texture
  std::unordered_map<ID3D12Resource*, winrt::com_ptr<ID3D12GraphicsCommandList>>
    mCommandLists;

  PixelSize mTextureDimensions;
  bool mHaveShaderResourceView {false};

  void InitializeCacheTexture(ID3D12Resource* sourceTexture) noexcept;
};

class CachedReader : public SHM::CachedReader, protected SHM::IPCTextureCopier {
 public:
  CachedReader() = delete;
  CachedReader(ConsumerKind);
  virtual ~CachedReader();

  void InitializeCache(
    ID3D12Device*,
    ID3D12CommandQueue* queue,
    uint8_t swapchainLength);

  virtual Snapshot MaybeGet(
    const std::source_location& loc = std::source_location::current()) override;

 protected:
  virtual void Copy(
    HANDLE sourceTexture,
    IPCClientTexture* destinationTexture,
    HANDLE fenceIn,
    uint64_t fenceInValue) noexcept override;

  virtual std::shared_ptr<SHM::IPCClientTexture> CreateIPCClientTexture(
    const PixelSize&,
    uint8_t swapchainIndex) noexcept override;

  virtual void ReleaseIPCHandles() override;

 private:
  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;

  std::vector<std::weak_ptr<Texture>> mCachedTextures;

  struct BufferResources {
    winrt::com_ptr<ID3D12CommandAllocator> mCommandAllocator;
  };

  std::vector<BufferResources> mBufferResources;
  std::unique_ptr<DirectX::DescriptorHeap> mShaderResourceViewHeap;

  uint8_t GetSwapchainLength() const;

  struct FenceAndValue {
    winrt::com_ptr<ID3D12Fence> mFence;
    uint64_t mValue {};

    inline operator bool() const noexcept {
      return mFence && mValue;
    }
  };
  std::unordered_map<HANDLE, FenceAndValue> mIPCFences;
  std::unordered_map<HANDLE, winrt::com_ptr<ID3D12Resource>> mIPCTextures;
  FenceAndValue mCopyFence;

  FenceAndValue* GetIPCFence(HANDLE) noexcept;
  ID3D12Resource* GetIPCTexture(HANDLE) noexcept;
  void WaitForPendingCopies();
};

}// namespace OpenKneeboard::SHM::D3D12