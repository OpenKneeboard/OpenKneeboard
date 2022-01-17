#pragma once

#include <dxgi.h>

#include <functional>
#include <memory>

namespace OpenKneeboard {

/** Hook for IDXGISwapChain::Present().
 *
 * This is called in both D3D11 and D3D12 apps; to determine which is being
 * used, check the type of `swapChain->GetDevice()` - it should be either an
 * `ID3D11Device` or `ID3D12Device`.
 */
class IDXGISwapChainPresentHook final {
 public:
  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<HRESULT(
      IDXGISwapChain* this_,
      UINT syncInterval,
      UINT flags,
      const decltype(&IDXGISwapChain::Present)& next)>
      onPresent;
  };
  IDXGISwapChainPresentHook() = delete;
  IDXGISwapChainPresentHook(const Callbacks& callbacks);
  ~IDXGISwapChainPresentHook();

  static inline auto make_unique(const Callbacks& callbacks) {
    return std::make_unique<IDXGISwapChainPresentHook>(callbacks);
  }

  void UninstallHook();

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
