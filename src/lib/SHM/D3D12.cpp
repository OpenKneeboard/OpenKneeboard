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

Reader::Reader(
  const ConsumerKind kind,
  ID3D12Device* const device,
  ID3D12CommandQueue* const queue)
  : SHM::Reader(kind, GetGPULUID(device)) {
  wil::com_query_to(device, mDevice.put());
  wil::com_copy_to(queue, mCommandQueue.put());

  mShaderResourceViewHeap = std::make_unique<DirectX::DescriptorHeap>(
    mDevice.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    SHM::SwapChainLength);
  mShaderResourceViewHeap->Heap()->SetName(L"OpenKneeboard::SHM::D3D12 SRV");
}

Reader::~Reader() { this->DropResources(); }

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
    com.mTextureDimensions = {
      static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height)};
  }
  if (!com.mShaderResourceViewGPUHandle.ptr) {
    const auto cpuHandle = mShaderResourceViewHeap->GetCpuHandle(raw.mIndex);
    mDevice->CreateShaderResourceView(com.mTexture.get(), nullptr, cpuHandle);
    com.mShaderResourceViewGPUHandle =
      mShaderResourceViewHeap->GetGpuHandle(raw.mIndex);
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

void Reader::DropResources() {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::Reader::DropResources");
  wil::com_ptr<ID3D12Fence> fence;
  check_hresult(
    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put())));
  check_hresult(mCommandQueue->Signal(fence.get(), 1));
  wil::unique_event e;
  e.create();
  check_hresult(fence->SetEventOnCompletion(1, e.get()));
  WaitForSingleObject(e.get(), INFINITE);

  mFrames = {};
}

void Reader::OnSessionChanged() {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::Reader::OnSessionChanged");
  this->DropResources();
}

}// namespace OpenKneeboard::SHM::D3D12
