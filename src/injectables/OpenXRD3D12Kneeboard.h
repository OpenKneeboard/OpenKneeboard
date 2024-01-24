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

#include "OpenXRKneeboard.h"

#include <OpenKneeboard/D3D12.h>
#include <OpenKneeboard/SHM/D3D12.h>

#include <OpenKneeboard/config.h>

#include <d3d12.h>

#include <directxtk12/DescriptorHeap.h>

struct XrGraphicsBindingD3D12KHR;

namespace OpenKneeboard {

class OpenXRD3D12Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D12Kneeboard() = delete;
  OpenXRD3D12Kneeboard(
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingD3D12KHR&);
  ~OpenXRD3D12Kneeboard();

 protected:
  virtual SHM::CachedReader* GetSHM() override;
  virtual XrSwapchain CreateSwapchain(
    XrSession,
    const PixelSize&,
    const VRRenderConfig::Quirks&) override;
  virtual void ReleaseSwapchainResources(XrSwapchain) override;
  virtual void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const PixelRect* const destRects,
    const float* const opacities) override;

 private:
  SHM::D3D12::CachedReader mSHM {SHM::ConsumerKind::OpenXR};

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;

  std::unique_ptr<D3D12::SpriteBatch> mSpriteBatch;

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

  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;
};

}// namespace OpenKneeboard
