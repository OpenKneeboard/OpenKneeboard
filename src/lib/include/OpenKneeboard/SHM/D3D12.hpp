// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/SHM.hpp>

#include <d3d12.h>

#include <wil/com.h>

#include <directxtk12/DescriptorHeap.h>

namespace OpenKneeboard::SHM::D3D12 {

struct Frame {
  using Error = SHM::Frame::Error;
  Config mConfig {};
  decltype(SHM::Frame::mLayers) mLayers {};

  ID3D12Resource* mTexture {};
  PixelSize mTextureDimensions {};

  ID3D12DescriptorHeap* mShaderResourceViewHeap {};
  D3D12_GPU_DESCRIPTOR_HANDLE mShaderResourceViewGPUHandle {};

  ID3D12Fence* mFence {};
  uint64_t mFenceIn {};
};

class Reader final : public SHM::Reader {
 public:
  Reader(ConsumerKind, ID3D12Device*, ID3D12CommandQueue*);
  ~Reader() override;

  Frame Map(SHM::Frame);
  std::expected<Frame, Frame::Error> MaybeGetMapped();

 protected:
  void OnSessionChanged() override;

 private:
  struct FrameD3D12Resources {
    HANDLE mTextureHandle {};
    wil::com_ptr<ID3D12Resource> mTexture;
    PixelSize mTextureDimensions {};
    D3D12_GPU_DESCRIPTOR_HANDLE mShaderResourceViewGPUHandle {};

    HANDLE mFenceHandle {};
    wil::com_ptr<ID3D12Fence> mFence;
  };

  wil::com_ptr<ID3D12Device> mDevice;
  wil::com_ptr<ID3D12CommandQueue> mCommandQueue;

  std::unique_ptr<DirectX::DescriptorHeap> mShaderResourceViewHeap;

  std::array<FrameD3D12Resources, SHM::SwapChainLength> mFrames {};
  void DropResources();
};

}// namespace OpenKneeboard::SHM::D3D12