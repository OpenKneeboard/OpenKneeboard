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

}// namespace OpenKneeboard::SHM::D3D12