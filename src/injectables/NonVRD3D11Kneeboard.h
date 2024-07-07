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

#include "IDXGISwapChainHook.h"

#include <OpenKneeboard/D3D11/Renderer.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/D3D11.h>

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
