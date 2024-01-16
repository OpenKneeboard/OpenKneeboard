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

#include "IDXGISwapChainPresentHook.h"
#include "OculusKneeboard.h"

#include <OpenKneeboard/SHM/D3D11.h>

#include <OpenKneeboard/config.h>

#include <shims/winrt/base.h>

#include <array>

#include <d3d11.h>
#include <d3d11_2.h>

namespace OpenKneeboard {

class OculusD3D11Kneeboard final : public OculusKneeboard::Renderer {
 public:
  OculusD3D11Kneeboard();
  virtual ~OculusD3D11Kneeboard();

  void UninstallHook();

  virtual SHM::ConsumerKind GetConsumerKind() const override {
    return SHM::ConsumerKind::OculusD3D11;
  }

  virtual SHM::CachedReader* GetSHM() override;

 protected:
  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    const PixelSize&) override;

  virtual bool RenderLayers(
    ovrSession session,
    ovrTextureSwapChain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    uint8_t layerCount,
    LayerRenderInfo* layers) override;

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() override;

 private:
  SHM::D3D11::CachedReader mSHM;

  using DeviceResources = SHM::D3D11::Renderer::DeviceResources;
  std::unique_ptr<DeviceResources> mDeviceResources;
  using SwapchainResources = SHM::D3D11::Renderer::SwapchainResources;
  std::unordered_map<ovrTextureSwapChain, std::unique_ptr<SwapchainResources>>
    mSwapchainResources;

  OculusKneeboard mOculusKneeboard;
  IDXGISwapChainPresentHook mDXGIHook;

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next);
};
}// namespace OpenKneeboard
