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

#include <OpenKneeboard/SHM/D3D11.h>

#include <OpenKneeboard/config.h>

#include <unordered_map>

struct XrGraphicsBindingD3D11KHR;

namespace OpenKneeboard {

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingD3D11KHR&);
  ~OpenXRD3D11Kneeboard();

  struct DXGIFormats {
    DXGI_FORMAT mTextureFormat;
    DXGI_FORMAT mRenderTargetViewFormat;
  };
  static DXGIFormats GetDXGIFormats(OpenXRNext*, XrSession);

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
    LayerRenderInfo* layers);

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() override;

 private:
  SHM::D3D11::CachedReader mSHM;

  using DeviceResources = SHM::D3D11::Renderer::DeviceResources;
  std::unique_ptr<DeviceResources> mDeviceResources;
  using SwapchainResources = SHM::D3D11::Renderer::SwapchainResources;
  std::unordered_map<XrSwapchain, std::unique_ptr<SwapchainResources>>
    mSwapchainResources;
};

}// namespace OpenKneeboard
