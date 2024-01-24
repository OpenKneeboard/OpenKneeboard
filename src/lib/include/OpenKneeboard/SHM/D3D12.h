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
    const winrt::com_ptr<ID3D12Device>&,
    ID3D12DescriptorHeap* shaderResourceViewHeap,
    const D3D12_CPU_DESCRIPTOR_HANDLE& shaderResourceViewCPUHandle,
    const D3D12_GPU_DESCRIPTOR_HANDLE& shaderResourceViewGPUHandle);
  virtual ~Texture();

  PixelSize GetDimensions() const;

  ID3D12Resource* GetD3D12Texture();
  D3D12_GPU_DESCRIPTOR_HANDLE GetD3D12ShaderResourceViewGPUHandle();
  ID3D12DescriptorHeap* GetD3D12ShaderResourceViewHeap();

  void CopyFrom(
    ID3D12CommandQueue*,
    ID3D12GraphicsCommandList*,
    HANDLE texture,
    HANDLE fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept;

 private:
  winrt::com_ptr<ID3D12Device> mDevice;

  winrt::com_ptr<ID3D12DescriptorHeap> mShaderResourceViewHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE mShaderResourceViewCPUHandle;
  D3D12_GPU_DESCRIPTOR_HANDLE mShaderResourceViewGPUHandle;

  winrt::com_ptr<ID3D12Resource> mTexture;
  PixelSize mTextureDimensions;
  bool mHaveShaderResourceView {false};

  HANDLE mSourceTextureHandle {};
  HANDLE mSourceFenceHandle {};

  winrt::com_ptr<ID3D12Resource> mSourceTexture;
  winrt::com_ptr<ID3D12Fence> mSourceFence;

  void InitializeSource(HANDLE texture, HANDLE fence) noexcept;
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

 protected:
  virtual void Copy(
    uint8_t swapchainIndex,
    HANDLE sourceTexture,
    IPCClientTexture* destinationTexture,
    HANDLE fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept override;

  virtual std::shared_ptr<SHM::IPCClientTexture> CreateIPCClientTexture(
    uint8_t swapchainIndex) noexcept override;

 private:
  winrt::com_ptr<ID3D12Device> mD3D12Device;
  winrt::com_ptr<ID3D12CommandQueue> mD3D12CommandQueue;

  struct BufferResources {
    winrt::com_ptr<ID3D12CommandAllocator> mD3D12CommandAllocator;
    winrt::com_ptr<ID3D12GraphicsCommandList> mD3D12CommandList;
  };

  std::vector<BufferResources> mBufferResources;
  std::unique_ptr<DirectX::DescriptorHeap> mShaderResourceViewHeap;

  uint8_t GetSwapchainLength() const;
};

}// namespace OpenKneeboard::SHM::D3D12