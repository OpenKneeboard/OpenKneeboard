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

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/GFXInterop.h>

#include <shims/winrt/base.h>

#include <memory>

#include <d3d11_4.h>
#include <d3d11on12.h>
#include <d3d12.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/GraphicsMemory.h>
#include <directxtk12/SpriteBatch.h>

namespace OpenKneeboard::SHM::D3D12 {

struct DeviceResources;

class LayerTextureCache : public SHM::GFXInterop::LayerTextureCache {
 public:
  LayerTextureCache() = delete;
  LayerTextureCache(
    uint8_t layerIndex,
    const winrt::com_ptr<ID3D11Texture2D>& d3d11Texture,
    const std::shared_ptr<DeviceResources>&);

  ID3D12Resource* GetD3D12Texture();
  D3D12_GPU_DESCRIPTOR_HANDLE GetD3D12ShaderResourceViewGPUHandle();

  virtual ~LayerTextureCache();

 private:
  std::shared_ptr<DeviceResources> mDeviceResources;
  uint8_t mLayerIndex;

  winrt::com_ptr<ID3D12Resource> mD3D12Texture;
  bool mHaveShaderResourceView {false};
};

class CachedReader : public SHM::CachedReader {
 public:
  using SHM::CachedReader::CachedReader;

  CachedReader() = delete;
  CachedReader(ID3D12Device*);
  virtual ~CachedReader();

  ID3D12DescriptorHeap* GetShaderResourceViewHeap() const;

 protected:
  virtual std::shared_ptr<SHM::LayerTextureCache> CreateLayerTextureCache(
    uint8_t layerIndex,
    const winrt::com_ptr<ID3D11Texture2D>&) override;

 private:
  std::shared_ptr<DeviceResources> mDeviceResources;
};

// Usage:
// - create DeviceResources and SwapchainResources
// - Call BeginFrame(), Render() [, Render(), ...], EndFrame()
// - Optionally call ClearRenderTargetView() after BeginFrame(), depending on
// your needs.
namespace Renderer {
struct DeviceResources {
  DeviceResources() = delete;
  DeviceResources(ID3D12Device*, ID3D12CommandQueue*);

  winrt::com_ptr<ID3D12Device> mD3D12Device;
  winrt::com_ptr<ID3D12CommandQueue> mD3D12CommandQueue;

  winrt::com_ptr<ID3D11Device5> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext4> mD3D11ImmediateContext;

  winrt::com_ptr<ID3D11On12Device> mD3D11On12Device;

  std::unique_ptr<DirectX::DescriptorHeap> mD3D12DepthHeap;
  std::unique_ptr<DirectX::GraphicsMemory> mDXTK12GraphicsMemory;

  winrt::com_ptr<ID3D12Fence> mD3D12Fence;
  winrt::handle mD3DInteropFenceHandle;
  winrt::com_ptr<ID3D11Fence> mD3D11Fence;
  uint64_t mFenceValue {};
  winrt::handle mFenceEvent;
};

struct SwapchainResources {
  SwapchainResources() = delete;
  SwapchainResources(
    DeviceResources*,
    DXGI_FORMAT renderTargetViewFormat,
    size_t textureCount,
    ID3D12Resource** textures);

  D3D12_VIEWPORT mViewport;
  D3D12_RECT mScissorRect;

  std::vector<winrt::com_ptr<ID3D12CommandAllocator>> mD3D12CommandAllocators;

  std::vector<winrt::com_ptr<ID3D12Resource>> mD3D12Textures;
  std::unique_ptr<DirectX::DescriptorHeap> mD3D12RenderTargetViewsHeap;
  winrt::com_ptr<ID3D12Resource> mD3D12DepthStencilTexture;

  std::unique_ptr<DirectX::DX12::SpriteBatch> mDXTK12SpriteBatch;
};

struct LayerSprite {
  uint8_t mLayerIndex {0xff};
  D3D12_RECT mDestRect {};
  FLOAT mOpacity {1.0f};
};

void BeginFrame(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

void ClearRenderTargetView(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

void Render(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex,
  const SHM::D3D12::CachedReader&,
  const SHM::Snapshot&,
  size_t layerSpriteCount,
  LayerSprite* layerSprites);

void EndFrame(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

}// namespace Renderer

}// namespace OpenKneeboard::SHM::D3D12