#pragma once

#include <d3d11.h>

namespace OpenKneeboard {

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

  virtual HRESULT OnPresent(
    UINT syncInterval,
    UINT flags,
    IDXGISwapChain* swapChain,
    const decltype(&IDXGISwapChain::Present)& next)
    = 0;
};

}// namespace OpenKneeboard
