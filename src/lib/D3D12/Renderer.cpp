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

  winrt::check_hresult(device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.put())));
  winrt::check_hresult(device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    mCommandAllocator.get(),
    nullptr,
    IID_PPV_ARGS(mCommandList.put())));
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
  const SwapchainResources& sr,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  const PixelRect* const destRects,
  const float* const opacities,
  RenderMode renderMode) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::Renderer::RenderLayers()");

  if (layerCount > snapshot.GetLayerCount()) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "Attempted to render more layers than exist in snapshot");
  }

  auto source = snapshot.GetTexture<SHM::D3D12::Texture>();
  auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  ID3D12DescriptorHeap* heaps[] {source->GetD3D12ShaderResourceViewHeap()};
  br.mCommandList->SetDescriptorHeaps(std::size(heaps), heaps);

  mSpriteBatch->Begin(
    br.mCommandList.get(), br.mRenderTargetView, sr.mDimensions);
  if (renderMode == RenderMode::ClearAndRender) {
    mSpriteBatch->Clear();
  }

  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    const auto sourceRect
      = snapshot.GetLayerConfig(layerIndex)->mLocationOnTexture;
    const auto& destRect = destRects[layerIndex];
    const D3D12::Opacity opacity {opacities[layerIndex]};
    mSpriteBatch->Draw(
      source->GetD3D12ShaderResourceViewGPUHandle(),
      source->GetDimensions(),
      sourceRect,
      destRects[layerIndex],
      D3D12::Opacity {opacities[layerIndex]});
  }
  mSpriteBatch->End();

  winrt::check_hresult(br.mCommandList->Close());

  ID3D12CommandList* lists[] {br.mCommandList.get()};

  mQueue->ExecuteCommandLists(std::size(lists), lists);
  winrt::check_hresult(
    br.mCommandList->Reset(br.mCommandAllocator.get(), nullptr));
}

}// namespace OpenKneeboard::D3D12