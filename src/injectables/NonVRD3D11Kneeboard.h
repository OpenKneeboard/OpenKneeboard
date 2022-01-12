#pragma once

#include <memory>

#include "IDXGISwapChainPresentHook.h"
#include "InjectedKneeboard.h"

namespace OpenKneeboard {

class NonVRD3D11Kneeboard final : public InjectedKneeboard,
                                  private IDXGISwapChainPresentHook {
 public:
  NonVRD3D11Kneeboard();
  virtual ~NonVRD3D11Kneeboard();

  virtual void Unhook() override;

 protected:
  virtual HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
