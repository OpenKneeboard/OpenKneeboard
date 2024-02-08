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
  const std::span<SHM::LayerSprite>& layers,
  RenderMode renderMode) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::Renderer::RenderLayers()");

  auto source = snapshot.GetTexture<SHM::D3D12::Texture>();
  auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  ID3D12DescriptorHeap* heaps[] {source->GetD3D12ShaderResourceViewHeap()};
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
  check_hresult(br.mCommandList->Reset(br.mCommandAllocator.get(), nullptr));
}

}// namespace OpenKneeboard::D3D12