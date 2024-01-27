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

#include <OpenKneeboard/D3D12.h>
#include <OpenKneeboard/RenderMode.h>
#include <OpenKneeboard/SHM/D3D12.h>

namespace OpenKneeboard::D3D12 {

struct SwapchainBufferResources {
  SwapchainBufferResources() = delete;
  SwapchainBufferResources(
    ID3D12Device*,
    ID3D12Resource* texture,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle,
    DXGI_FORMAT renderTargetViewFormat);

  winrt::com_ptr<ID3D12CommandAllocator> mCommandAllocator;
  winrt::com_ptr<ID3D12GraphicsCommandList> mCommandList;

  D3D12_CPU_DESCRIPTOR_HANDLE mRenderTargetView;
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
    const SwapchainResources&,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    uint8_t layerCount,
    const PixelRect* const destRects,
    const float* const opacities,
    RenderMode);

 private:
  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mQueue;
  std::unique_ptr<SpriteBatch> mSpriteBatch;
};

}// namespace OpenKneeboard::D3D12