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
#pragma once

#include <OpenKneeboard/SHM.h>

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
    ID3D12DescriptorHeap* shaderResourceViewHeap,
    const D3D12_CPU_DESCRIPTOR_HANDLE& shaderResourceViewCPUHandle,
    const D3D12_GPU_DESCRIPTOR_HANDLE& shaderResourceViewGPUHandle);
  virtual ~Texture();

  ID3D12Resource* GetD3D12Texture();
  D3D12_GPU_DESCRIPTOR_HANDLE GetD3D12ShaderResourceViewGPUHandle();
  ID3D12DescriptorHeap* GetD3D12ShaderResourceViewHeap();

  void CopyFrom(
    ID3D12CommandQueue*,
    ID3D12CommandAllocator*,
    ID3D12Resource* texture,
    ID3D12Fence* fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept;

 private:
  winrt::com_ptr<ID3D12Device> mDevice;

  winrt::com_ptr<ID3D12DescriptorHeap> mShaderResourceViewHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE mShaderResourceViewCPUHandle;
  D3D12_GPU_DESCRIPTOR_HANDLE mShaderResourceViewGPUHandle;

  void PopulateCommandList(
    ID3D12GraphicsCommandList*,
    ID3D12Resource* texture) noexcept;

  std::unordered_map<ID3D12Resource*, winrt::com_ptr<ID3D12GraphicsCommandList>>
    mCommandLists;

  winrt::com_ptr<ID3D12Resource> mTexture;
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
    HANDLE fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept override;

  virtual std::shared_ptr<SHM::IPCClientTexture> CreateIPCClientTexture(
    const PixelSize&,
    uint8_t swapchainIndex) noexcept override;

 private:
  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;

  struct BufferResources {
    winrt::com_ptr<ID3D12CommandAllocator> mCommandAllocator;
  };

  std::vector<BufferResources> mBufferResources;
  std::unique_ptr<DirectX::DescriptorHeap> mShaderResourceViewHeap;

  uint8_t GetSwapchainLength() const;

  std::unordered_map<HANDLE, winrt::com_ptr<ID3D12Fence>> mIPCFences;
  std::unordered_map<HANDLE, winrt::com_ptr<ID3D12Resource>> mIPCTextures;

  ID3D12Fence* GetIPCFence(HANDLE) noexcept;
  ID3D12Resource* GetIPCTexture(HANDLE) noexcept;
};

}// namespace OpenKneeboard::SHM::D3D12