// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/D3D12.hpp>
#include <OpenKneeboard/RenderDoc.hpp>
#include <OpenKneeboard/SHM/D3D12.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <Windows.h>

#include <DirectXColors.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

namespace OpenKneeboard::SHM::D3D12 {

namespace {

[[nodiscard]]
uint64_t GetGPULUID(ID3D12Device* const device) {
  return std::bit_cast<uint64_t>(device->GetAdapterLuid());
}

}// namespace

Reader::Reader(const ConsumerKind kind, ID3D12Device* const device)
  : SHM::Reader(kind, GetGPULUID(device)) {
  wil::com_query_to(device, mDevice.put());

  mShaderResourceViewHeap = std::make_unique<DirectX::DescriptorHeap>(
    mDevice.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    SHM::SwapChainLength);
  mShaderResourceViewHeap->Heap()->SetName(L"OpenKneeboard::SHM::D3D12 SRV");
}

Reader::~Reader() = default;

std::expected<Frame, Frame::Error> Reader::MaybeGetMapped() {
  auto raw = MaybeGet();
  if (!raw) {
    return std::unexpected {raw.error()};
  }
  return Map(*std::move(raw));
}

Frame Reader::Map(SHM::Frame raw) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::Reader::Map");
  auto& com = mFrames.at(raw.mIndex);
  if (com.mTextureHandle != raw.mTexture) {
    com.mTextureHandle = raw.mTexture;
    com.mTexture.reset();
    com.mTextureDimensions = {};
    com.mShaderResourceViewGPUHandle = {};
  }
  if (com.mFenceHandle != raw.mFence) {
    com.mFenceHandle = raw.mFence;
    com.mFence.reset();
  }

  if (!com.mTexture) {
    check_hresult(mDevice->OpenSharedHandle(
      com.mTextureHandle, IID_PPV_ARGS(com.mTexture.put())));
    const auto desc = com.mTexture->GetDesc();
    com.mTextureDimensions
      = {static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height)};
  }
  if (!com.mShaderResourceViewGPUHandle.ptr) {
    const auto cpuHandle = mShaderResourceViewHeap->GetCpuHandle(raw.mIndex);
    mDevice->CreateShaderResourceView(com.mTexture.get(), nullptr, cpuHandle);
    com.mShaderResourceViewGPUHandle
      = mShaderResourceViewHeap->GetGpuHandle(raw.mIndex);
  }
  if (!com.mFence) {
    check_hresult(mDevice->OpenSharedHandle(
      com.mFenceHandle, IID_PPV_ARGS(com.mFence.put())));
  }

  return Frame {
    .mConfig = std::move(raw.mConfig),
    .mLayers = std::move(raw.mLayers),
    .mTexture = com.mTexture.get(),
    .mTextureDimensions = com.mTextureDimensions,
    .mShaderResourceViewHeap = mShaderResourceViewHeap->Heap(),
    .mShaderResourceViewGPUHandle = com.mShaderResourceViewGPUHandle,
    .mFence = com.mFence.get(),
    .mFenceIn = std::bit_cast<uint64_t>(raw.mFenceIn),
  };
}

void Reader::OnSessionChanged() {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::Reader::OnSessionChanged");
  mFrames = {};
}

}// namespace OpenKneeboard::SHM::D3D12