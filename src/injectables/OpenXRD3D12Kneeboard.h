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

#include <OpenKneeboard/SHM/D3D12.h>

#include <OpenKneeboard/config.h>

#include <d3d11_4.h>
#include <d3d11on12.h>
#include <d3d12.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/GraphicsMemory.h>
#include <directxtk12/SpriteBatch.h>

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
  virtual bool ConfigurationsAreCompatible(
    const VRRenderConfig& initial,
    const VRRenderConfig& current) const override;
  virtual XrSwapchain CreateSwapchain(
    XrSession,
    const PixelSize&,
    const VRRenderConfig::Quirks&) override;
  virtual void ReleaseSwapchainResources(XrSwapchain) override;
  virtual bool RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    uint8_t layerCount,
    LayerRenderInfo* layers) override;

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() override;

 private:
  SHM::D3D12::CachedReader mSHM;
  ///// START Device Resources /////
  winrt::com_ptr<ID3D12Device> mD3D12Device;
  winrt::com_ptr<ID3D12CommandQueue> mD3D12CommandQueue;
  winrt::com_ptr<ID3D12CommandAllocator> mD3D12CommandAllocator;

  winrt::com_ptr<ID3D11Device5> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext4> mD3D11ImmediateContext;

  winrt::com_ptr<ID3D11On12Device> mD3D11On12Device;

  std::unique_ptr<DirectX::DescriptorHeap> mD3D12DepthHeap;
  std::unique_ptr<DirectX::GraphicsMemory> mDXTK12GraphicsMemory;

  winrt::com_ptr<ID3D12RootSignature> mD3D12GraphicsRootSignature;

  winrt::com_ptr<ID3D12Fence> mD3D12Fence;
  winrt::handle mD3DInteropFenceHandle;
  winrt::com_ptr<ID3D11Fence> mD3D11Fence;
  uint64_t mFenceValue {};
  winrt::handle mFenceEvent;

  void InitializeDeviceResources(const XrGraphicsBindingD3D12KHR&);
  ///// END Device Resources /////

  ///// START swapchain resources /////
  struct SwapchainResources {
    D3D12_VIEWPORT mViewport;
    D3D12_RECT mScissorRect;

    DXGI_FORMAT mRenderTargetViewFormat;

    std::vector<winrt::com_ptr<ID3D12Resource>> mD3D12Textures;
    std::unique_ptr<DirectX::DescriptorHeap> mD3D12RenderTargetViewsHeap;
    winrt::com_ptr<ID3D12Resource> mD3D12DepthStencilTexture;

    std::unique_ptr<DirectX::DX12::SpriteBatch> mDXTK12SpriteBatch;
  };
  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;
  ///// END swapchain resources /////
};

}// namespace OpenKneeboard
