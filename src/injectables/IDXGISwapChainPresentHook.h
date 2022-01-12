#pragma once

#include <dxgi.h>

namespace OpenKneeboard {

/** Hook for IDXGISwapChain::Present().
 *
 * This is called in both D3D11 and D3D12 apps; to determine which is being
 * used, check the type of `swapChain->GetDevice()` - it should be either an
 * `ID3D11Device` or `ID3D12Device`.
 */
class IDXGISwapChainPresentHook {
 private:
  class Impl;

 public:
  IDXGISwapChainPresentHook();
  ~IDXGISwapChainPresentHook();
  void Unhook();
  // Unhook and remove all references to this instance
  // This is unsafe unless you know that all calls will be from the same thread
  void UnhookAndCleanup();

 protected:
  bool IsHookInstalled() const;

  virtual HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next)
    = 0;
};

}// namespace OpenKneeboard
