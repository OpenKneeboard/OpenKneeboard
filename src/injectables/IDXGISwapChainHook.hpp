// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <functional>
#include <memory>

#include <dxgi.h>

namespace OpenKneeboard {

/** Hook for IDXGISwapChain::Present().
 *
 * This is called in both D3D11 and D3D12 apps; to determine which is being
 * used, check the type of `swapChain->GetDevice()` - it should be either an
 * `ID3D11Device` or `ID3D12Device`.
 */
class IDXGISwapChainHook final {
 public:
  IDXGISwapChainHook();
  ~IDXGISwapChainHook();

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<HRESULT(
      IDXGISwapChain* this_,
      UINT syncInterval,
      UINT flags,
      const decltype(&IDXGISwapChain::Present)& next)>
      onPresent;
    std::function<HRESULT(
      IDXGISwapChain* this_,
      UINT bufferCount,
      UINT width,
      UINT height,
      DXGI_FORMAT newFormat,
      UINT swapChainFlags,
      const decltype(&IDXGISwapChain::ResizeBuffers)& next)>
      onResizeBuffers;
  };

  void InstallHook(const Callbacks& callbacks);
  void UninstallHook();

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
