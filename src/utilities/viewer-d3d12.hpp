// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "viewer.hpp"

#include <OpenKneeboard/D3D12.hpp>
#include <OpenKneeboard/SHM/D3D12.hpp>

#include <shims/winrt/base.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/GraphicsMemory.h>

namespace OpenKneeboard::Viewer {

class D3D12Renderer final : public Renderer {
 public:
  D3D12Renderer() = delete;
  D3D12Renderer(IDXGIAdapter*);
  virtual ~D3D12Renderer();
  virtual SHM::CachedReader* GetSHM() override;

  std::wstring_view GetName() const noexcept override;

  virtual void Initialize(uint8_t swapchainLength) override;

  virtual void SaveTextureToFile(
    SHM::IPCClientTexture*,
    const std::filesystem::path&) override;

  virtual uint64_t Render(
    SHM::IPCClientTexture* sourceTexture,
    const PixelRect& sourceRect,
    HANDLE destTexture,
    const PixelSize& destTextureDimensions,
    const PixelRect& destRect,
    HANDLE fence,
    uint64_t fenceValueIn) override;

 private:
  SHM::D3D12::CachedReader mSHM {SHM::ConsumerKind::Viewer};

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;
  winrt::com_ptr<ID3D12CommandAllocator> mCommandAllocator;
  winrt::com_ptr<ID3D12GraphicsCommandList> mCommandList;

  std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
  std::unique_ptr<D3D12::SpriteBatch> mSpriteBatch;

  HANDLE mDestHandle {};
  PixelSize mDestDimensions;
  winrt::com_ptr<ID3D12Resource> mDestTexture;
  std::unique_ptr<DirectX::DescriptorHeap> mDestRTVHeap;

  HANDLE mFenceHandle {};
  winrt::com_ptr<ID3D12Fence> mFence;

  uint64_t mFenceValue {};

  void SaveTextureToFile(ID3D12Resource*, const std::filesystem::path&);
};

}// namespace OpenKneeboard::Viewer