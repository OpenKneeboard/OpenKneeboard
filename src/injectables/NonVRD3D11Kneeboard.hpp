// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "IDXGISwapChainHook.hpp"

#include <OpenKneeboard/D3D11/Renderer.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/SHM/D3D11.hpp>

#include <shims/winrt/base.h>

#include <memory>

#include <d3d11_4.h>

namespace OpenKneeboard {

class NonVRD3D11Kneeboard final {
 public:
  NonVRD3D11Kneeboard();
  virtual ~NonVRD3D11Kneeboard();

  void UninstallHook();

 private:
  SHM::D3D11::CachedReader mSHM {SHM::ConsumerKind::NonVRD3D11};
  IDXGISwapChainHook mPresentHook;

  using SwapchainResources = D3D11::SwapchainResources;
  using SwapchainBufferResources = D3D11::SwapchainBufferResources;

  struct Resources {
    ID3D11Device* mDevice {nullptr};
    winrt::com_ptr<ID3D11DeviceContext1> mImmediateContext;
    IDXGISwapChain* mSwapchain {nullptr};
    SwapchainResources mSwapchainResources;
    std::unique_ptr<D3D11::Renderer> mRenderer;

    D3D11::DeviceContextState mRenderState;
  };
  std::optional<Resources> mResources;

  void InitializeResources(IDXGISwapChain*);

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next);

  HRESULT OnIDXGISwapChain_ResizeBuffers(
    IDXGISwapChain* this_,
    UINT bufferCount,
    UINT width,
    UINT height,
    DXGI_FORMAT newFormat,
    UINT swapChainFlags,
    const decltype(&IDXGISwapChain::ResizeBuffers)& next);
};

}// namespace OpenKneeboard
