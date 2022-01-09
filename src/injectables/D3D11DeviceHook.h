#pragma once

#include <winrt/base.h>

#include "IDXGISwapChainPresentHook.h"

namespace OpenKneeboard {

class D3D11DeviceHook final : public IDXGISwapChainPresentHook {
 private:
  winrt::com_ptr<ID3D11Device> mD3D;

 public:
  D3D11DeviceHook();
  ~D3D11DeviceHook();
  winrt::com_ptr<ID3D11Device> MaybeGet();

 protected:
  virtual HRESULT OnPresent(
    UINT syncInterval,
    UINT flags,
    IDXGISwapChain* swapChain,
    decltype(&IDXGISwapChain::Present) next) override;
};

}// namespace OpenKneeboard
