// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/D3D12.hpp>
#include <OpenKneeboard/RenderMode.hpp>
#include <OpenKneeboard/SHM/D3D12.hpp>

#include <span>

namespace OpenKneeboard::D3D12 {

struct SwapchainBufferResources {
  SwapchainBufferResources() = delete;
  SwapchainBufferResources(const SwapchainBufferResources&) = delete;
  SwapchainBufferResources& operator=(const SwapchainBufferResources&) = delete;

  SwapchainBufferResources(SwapchainBufferResources&&);
  SwapchainBufferResources& operator=(SwapchainBufferResources&&);

  SwapchainBufferResources(
    ID3D12Device*,
    ID3D12Resource* texture,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle,
    DXGI_FORMAT renderTargetViewFormat);
  ~SwapchainBufferResources();

  winrt::com_ptr<ID3D12CommandAllocator> mCommandAllocator;
  winrt::com_ptr<ID3D12GraphicsCommandList> mCommandList;
  winrt::com_ptr<ID3D12Fence> mFence;
  uint64_t mFenceValue {};

  D3D12_CPU_DESCRIPTOR_HANDLE mRenderTargetView {};
};

struct SwapchainResources {
  PixelSize mDimensions;
  DirectX::DescriptorHeap mRenderTargetViewHeap;
  std::vector<SwapchainBufferResources> mBufferResources;
};

class Renderer {
 public:
  Renderer() = delete;
  Renderer(ID3D12Device*, ID3D12CommandQueue*, DXGI_FORMAT destFormat);

  void RenderLayers(
    SwapchainResources&,
    uint32_t swapchainTextureIndex,
    const SHM::D3D12::Frame& frame,
    const std::span<SHM::LayerSprite>& layers,
    RenderMode);

 private:
  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mQueue;
  std::unique_ptr<SpriteBatch> mSpriteBatch;
};

}// namespace OpenKneeboard::D3D12
