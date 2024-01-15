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

#include <d3d12.h>

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

  ID3D12DescriptorHeap* GetShaderResourceViewHeap();

 protected:
  virtual std::shared_ptr<SHM::LayerTextureCache> CreateLayerTextureCache(
    uint8_t layerIndex,
    const winrt::com_ptr<ID3D11Texture2D>&) override;

 private:
  std::shared_ptr<DeviceResources> mDeviceResources;
};

};// namespace OpenKneeboard::SHM::D3D12