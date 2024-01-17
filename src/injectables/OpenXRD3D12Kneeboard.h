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
  virtual void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    uint8_t layerCount,
    SHM::LayerSprite* layers) override;

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() override;

 private:
  SHM::D3D12::CachedReader mSHM;

  using DeviceResources = SHM::D3D12::Renderer::DeviceResources;
  std::unique_ptr<DeviceResources> mDeviceResources;

  using SwapchainResources = SHM::D3D12::Renderer::SwapchainResources;
  std::unordered_map<XrSwapchain, std::unique_ptr<SwapchainResources>>
    mSwapchainResources;
};

}// namespace OpenKneeboard
