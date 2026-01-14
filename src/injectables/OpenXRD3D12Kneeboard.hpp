// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "OpenXRKneeboard.hpp"

#include <OpenKneeboard/D3D12.hpp>
#include <OpenKneeboard/D3D12/Renderer.hpp>
#include <OpenKneeboard/SHM/D3D12.hpp>

#include <OpenKneeboard/config.hpp>

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
    const std::span<SHM::LayerSprite>& layers) override;

 private:
  SHM::D3D12::CachedReader mSHM {SHM::ConsumerKind::OpenXR_D3D12};

  std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;

  using SwapchainBufferResources = D3D12::SwapchainBufferResources;
  using SwapchainResources = D3D12::SwapchainResources;

  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;
  std::unique_ptr<D3D12::Renderer> mRenderer;
};

}// namespace OpenKneeboard
