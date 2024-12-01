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

#include <OpenKneeboard/D3D12/Renderer.h>

#include <OpenKneeboard/hresult.h>

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
  check_hresult(
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.put())));
}

SwapchainBufferResources::~SwapchainBufferResources() {
  if (mFenceValue && mFence->GetCompletedValue() < mFenceValue) {
    winrt::handle event {CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    check_hresult(mFence->SetEventOnCompletion(mFenceValue, event.get()));
    const auto code = WaitForSingleObject(event.get(), 5000);
    if (code != WAIT_OBJECT_0) [[unlikely]] {
      OPENKNEEBOARD_LOG_AND_FATAL("Waiting for fence failed with {:#010x}", static_cast<uintptr_t>(code));
    }
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
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  const std::span<SHM::LayerSprite>& layers,
  RenderMode renderMode) {
  OPENKNEEBOARD_TraceLoggingScope(
    "D3D12::Renderer::RenderLayers()",
    TraceLoggingValue(swapchainTextureIndex, "swapchainTextureIndex"));

  auto source = snapshot.GetTexture<SHM::D3D12::Texture>();
  auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  ID3D12DescriptorHeap* heaps[] {source->GetD3D12ShaderResourceViewHeap()};
  if (br.mFenceValue) {
    if (br.mFence->GetCompletedValue() < br.mFenceValue) [[unlikely]] {
      throw std::runtime_error("Swapchain buffers out of order");
    }
    check_hresult(br.mCommandAllocator->Reset());
    check_hresult(br.mCommandList->Reset(br.mCommandAllocator.get(), nullptr));
  }

  br.mCommandList->SetDescriptorHeaps(std::size(heaps), heaps);

  mSpriteBatch->Begin(
    br.mCommandList.get(), br.mRenderTargetView, sr.mDimensions);
  if (renderMode == RenderMode::ClearAndRender) {
    mSpriteBatch->Clear();
  }

  const auto baseTint = snapshot.GetConfig().mTint;

  for (const auto& layer: layers) {
    const DirectX::XMVECTORF32 layerTint {
      baseTint[0] * layer.mOpacity,
      baseTint[1] * layer.mOpacity,
      baseTint[2] * layer.mOpacity,
      baseTint[3] * layer.mOpacity,
    };
    mSpriteBatch->Draw(
      source->GetD3D12ShaderResourceViewGPUHandle(),
      source->GetDimensions(),
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
