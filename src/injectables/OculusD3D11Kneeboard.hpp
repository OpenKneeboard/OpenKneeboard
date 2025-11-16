// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "IDXGISwapChainHook.hpp"
#include "OculusKneeboard.hpp"

#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/D3D11/Renderer.hpp>
#include <OpenKneeboard/SHM/D3D11.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/config.hpp>

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
  winrt::com_ptr<ID3D11DeviceContext1> mD3D11DeviceContext;
  std::unique_ptr<D3D11::Renderer> mRenderer;
  D3D11::DeviceContextState mRenderState;

  using SwapchainBufferResources = D3D11::SwapchainBufferResources;
  using SwapchainResources = D3D11::SwapchainResources;

  std::optional<SwapchainResources> mSwapchain;

  SHM::D3D11::CachedReader mSHM {SHM::ConsumerKind::OculusD3D11};

  OculusKneeboard mOculusKneeboard;
  IDXGISwapChainHook mDXGIHook;

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next) noexcept;
};
}// namespace OpenKneeboard
