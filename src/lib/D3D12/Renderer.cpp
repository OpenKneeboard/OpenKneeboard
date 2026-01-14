// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/D3D12/Renderer.hpp>

#include <OpenKneeboard/hresult.hpp>

namespace OpenKneeboard::D3D12 {

SwapchainBufferResources::SwapchainBufferResources(
  ID3D12Device* device,
  ID3D12Resource* texture,
  D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle,
  DXGI_FORMAT renderTargetViewFormat)
  : mRenderTargetView(renderTargetViewHandle) {
  D3D12_RENDER_TARGET_VIEW_DESC desc {
    .Format = renderTargetViewFormat,
    .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {0, 0},
  };

  device->CreateRenderTargetView(texture, &desc, renderTargetViewHandle);

  check_hresult(device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.put())));
  check_hresult(device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    mCommandAllocator.get(),
    nullptr,
    IID_PPV_ARGS(mCommandList.put())));
  mCommandList->SetName(
    L"OpenKneeboard::D3D12::SwapchainBufferResources::"
    L"SwapchainBufferResources");
  check_hresult(
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.put())));
}

SwapchainBufferResources::~SwapchainBufferResources() {
  if (mFenceValue && mFence->GetCompletedValue() < mFenceValue) {
    winrt::handle event {CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    check_hresult(mFence->SetEventOnCompletion(mFenceValue, event.get()));
    const auto waitResult = WaitForSingleObject(event.get(), 5000);
    OPENKNEEBOARD_ALWAYS_ASSERT(
      waitResult == WAIT_OBJECT_0,
      "Wait result: {:#010x}",
      static_cast<uintptr_t>(waitResult));
  }
}

SwapchainBufferResources::SwapchainBufferResources(
  SwapchainBufferResources&& other) {
  this->operator=(std::move(other));
}

SwapchainBufferResources& SwapchainBufferResources::operator=(
  SwapchainBufferResources&& other) {
  mCommandAllocator = std::move(other.mCommandAllocator);
  mCommandList = std::move(other.mCommandList);
  mFence = std::move(other.mFence);
  mFenceValue = std::move(other.mFenceValue);
  mRenderTargetView = std::move(other.mRenderTargetView);

  other.mFenceValue = {};
  other.mRenderTargetView = {};

  return *this;
}

Renderer::Renderer(
  ID3D12Device* device,
  ID3D12CommandQueue* commandQueue,
  DXGI_FORMAT destFormat) {
  mSpriteBatch
    = std::make_unique<SpriteBatch>(device, commandQueue, destFormat);

  mDevice.copy_from(device);
  mQueue.copy_from(commandQueue);
}

void Renderer::RenderLayers(
  SwapchainResources& sr,
  const uint32_t swapchainTextureIndex,
  const SHM::D3D12::Frame& frame,
  const std::span<SHM::LayerSprite>& layers,
  const RenderMode renderMode) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::Renderer::RenderLayers()");

  auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  ID3D12DescriptorHeap* heaps[] {frame.mShaderResourceViewHeap};
  if (br.mFenceValue) {
    const auto minimumValue = br.mFenceValue;
    const auto actualValue = br.mFence->GetCompletedValue();
    OPENKNEEBOARD_ALWAYS_ASSERT(
      actualValue >= minimumValue,
      "Required {} >= {}",
      actualValue,
      minimumValue);
    check_hresult(br.mCommandAllocator->Reset());
    check_hresult(br.mCommandList->Reset(br.mCommandAllocator.get(), nullptr));
  }

  check_hresult(mQueue->Wait(frame.mFence, frame.mFenceIn));

  br.mCommandList->SetDescriptorHeaps(std::size(heaps), heaps);

  mSpriteBatch->Begin(
    br.mCommandList.get(), br.mRenderTargetView, sr.mDimensions);

  if (renderMode == RenderMode::ClearAndRender) {
    mSpriteBatch->Clear();
  }

  const auto baseTint = frame.mConfig.mTint;

  for (const auto& layer: layers) {
    const DirectX::XMVECTORF32 layerTint {
      baseTint[0] * layer.mOpacity,
      baseTint[1] * layer.mOpacity,
      baseTint[2] * layer.mOpacity,
      baseTint[3] * layer.mOpacity,
    };
    mSpriteBatch->Draw(
      frame.mTexture,
      frame.mTextureDimensions,
      layer.mSourceRect,
      layer.mDestRect,
      layerTint);
  }
  mSpriteBatch->End();

  check_hresult(br.mCommandList->Close());

  ID3D12CommandList* lists[] {br.mCommandList.get()};

  mQueue->ExecuteCommandLists(std::size(lists), lists);
  check_hresult(mQueue->Signal(br.mFence.get(), ++br.mFenceValue));
}

}// namespace OpenKneeboard::D3D12