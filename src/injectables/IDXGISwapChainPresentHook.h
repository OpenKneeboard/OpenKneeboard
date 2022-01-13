#pragma once

#include <dxgi.h>

#include <memory>

namespace OpenKneeboard {

/** Hook for IDXGISwapChain::Present().
 *
 * This is called in both D3D11 and D3D12 apps; to determine which is being
 * used, check the type of `swapChain->GetDevice()` - it should be either an
 * `ID3D11Device` or `ID3D12Device`.
 */
class IDXGISwapChainPresentHook {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  ~IDXGISwapChainPresentHook();

  void UninstallHook();

 protected:
  IDXGISwapChainPresentHook();
  void InitWithVTable();

  virtual HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next)
    = 0;
};

}// namespace OpenKneeboard
