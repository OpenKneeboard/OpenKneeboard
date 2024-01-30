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
#include <OpenKneeboard/D3D12/Renderer.h>
#include <OpenKneeboard/SHM/D3D12.h>

#include <OpenKneeboard/config.h>

#include <d3d12.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/GraphicsMemory.h>

struct XrGraphicsBindingD3D12KHR;

namespace OpenKneeboard {

class OpenXRD3D12Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D12Kneeboard() = delete;
  OpenXRD3D12Kneeboard(
    XrInstance,
    XrSystemId,
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingD3D12KHR&);
  ~OpenXRD3D12Kneeboard();

 protected:
  virtual SHM::CachedReader* GetSHM() override;
  virtual XrSwapchain CreateSwapchain(XrSession, const PixelSize&) override;
  virtual void ReleaseSwapchainResources(XrSwapchain) override;
  virtual void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const std::span<SHM::LayerRenderInfo>& layers) override;

 private:
  SHM::D3D12::CachedReader mSHM {SHM::ConsumerKind::OpenXR};

  std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;

  using SwapchainBufferResources = D3D12::SwapchainBufferResources;
  using SwapchainResources = D3D12::SwapchainResources;

  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;
  std::unique_ptr<D3D12::Renderer> mRenderer;
};

}// namespace OpenKneeboard
