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

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/D3D11/Renderer.h>
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

  virtual SHM::CachedReader* GetSHM() override;

  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    const PixelSize&) override;

  virtual void RenderLayers(
    ovrTextureSwapChain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const std::span<SHM::LayerSprite>& layers) override;

 private:
  winrt::com_ptr<ID3D11Device> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext> mD3D11DeviceContext;
  std::unique_ptr<D3D11::Renderer> mRenderer;

  using SwapchainBufferResources = D3D11::SwapchainBufferResources;
  using SwapchainResources = D3D11::SwapchainResources;

  std::optional<SwapchainResources> mSwapchain;

  SHM::D3D11::CachedReader mSHM {SHM::ConsumerKind::OculusD3D11};

  OculusKneeboard mOculusKneeboard;
  IDXGISwapChainPresentHook mDXGIHook;

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next) noexcept;
};
}// namespace OpenKneeboard
