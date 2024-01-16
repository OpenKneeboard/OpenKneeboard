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
#include <OpenKneeboard/SHM/D3D12.h>

#include <directxtk12/DescriptorHeap.h>

namespace OpenKneeboard::SHM::D3D12 {

struct DeviceResources {
  winrt::com_ptr<ID3D12Device> mDevice;
  DirectX::DescriptorHeap mSRVHeap;
};

LayerTextureCache::LayerTextureCache(
  uint8_t layerIndex,
  const winrt::com_ptr<ID3D11Texture2D>& d3d11Texture,
  const std::shared_ptr<DeviceResources>& resources)
  : SHM::GFXInterop::LayerTextureCache(d3d11Texture),
    mLayerIndex(layerIndex),
    mDeviceResources(resources) {
}

ID3D12Resource* LayerTextureCache::GetD3D12Texture() {
  if (!mD3D12Texture) [[unlikely]] {
    winrt::check_hresult(mDeviceResources->mDevice->OpenSharedHandle(
      this->GetNTHandle(), IID_PPV_ARGS(mD3D12Texture.put())));
  }
  return mD3D12Texture.get();
}

D3D12_GPU_DESCRIPTOR_HANDLE
LayerTextureCache::GetD3D12ShaderResourceViewGPUHandle() {
  if (!mHaveShaderResourceView) [[unlikely]] {
    mDeviceResources->mDevice->CreateShaderResourceView(
      this->GetD3D12Texture(),
      nullptr,
      mDeviceResources->mSRVHeap.GetCpuHandle(mLayerIndex));
    mHaveShaderResourceView = true;
  }

  return mDeviceResources->mSRVHeap.GetGpuHandle(mLayerIndex);
}

LayerTextureCache::~LayerTextureCache() = default;

CachedReader::CachedReader(ID3D12Device* devicePtr) {
  winrt::com_ptr<ID3D12Device> device;
  device.copy_from(devicePtr);

  DirectX::DescriptorHeap srvHeap(device.get(), MaxLayers);

  mDeviceResources
    = std::make_shared<DeviceResources>(std::move(device), std::move(srvHeap));
}

CachedReader::~CachedReader() = default;

ID3D12DescriptorHeap* CachedReader::GetShaderResourceViewHeap() {
  return mDeviceResources->mSRVHeap.Heap();
}

std::shared_ptr<SHM::LayerTextureCache> CachedReader::CreateLayerTextureCache(
  uint8_t layerIndex,
  const winrt::com_ptr<ID3D11Texture2D>& texture) {
  return std::make_shared<SHM::D3D12::LayerTextureCache>(
    layerIndex, texture, mDeviceResources);
}

namespace Renderer {

DeviceResources::DeviceResources(
  ID3D12Device* device,
  ID3D12CommandQueue* queue) {
  mD3D12Device.copy_from(device);
  mD3D12CommandQueue.copy_from(queue);
  winrt::check_hresult(mD3D12Device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    IID_PPV_ARGS(mD3D12CommandAllocator.put())));

  UINT flags = 0;
#ifdef DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  winrt::com_ptr<ID3D11Device> device11;
  winrt::com_ptr<ID3D11DeviceContext> context11;
  D3D11On12CreateDevice(
    mD3D12Device.get(),
    flags,
    nullptr,
    0,
    nullptr,
    0,
    1,
    device11.put(),
    context11.put(),
    nullptr);
  mD3D11Device = device11.as<ID3D11Device5>();
  mD3D11ImmediateContext = context11.as<ID3D11DeviceContext4>();
  mD3D11On12Device = device11.as<ID3D11On12Device>();

  mD3D12DepthHeap = std::make_unique<DirectX::DescriptorHeap>(
    mD3D12Device.get(),
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    1);

  mDXTK12GraphicsMemory
    = std::make_unique<DirectX::GraphicsMemory>(mD3D12Device.get());

  mD3D12Device->CreateFence(
    0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(mD3D12Fence.put()));
  mFenceEvent.attach(CreateEventEx(nullptr, nullptr, 0, GENERIC_ALL));
  winrt::check_hresult(mD3D12Device->CreateSharedHandle(
    mD3D12Fence.get(),
    nullptr,
    GENERIC_ALL,
    nullptr,
    mD3DInteropFenceHandle.put()));
  winrt::check_hresult(mD3D11Device->OpenSharedFence(
    mD3DInteropFenceHandle.get(), IID_PPV_ARGS(mD3D11Fence.put())));
}

}// namespace Renderer

}// namespace OpenKneeboard::SHM::D3D12