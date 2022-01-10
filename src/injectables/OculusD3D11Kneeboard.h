#pragma once

#include "IDXGISwapChainPresentHook.h"
#include "OculusKneeboard.h"

namespace OpenKneeboard {

class OculusD3D11Kneeboard final : public OculusKneeboard,
                                   private IDXGISwapChainPresentHook {
 public:
  OculusD3D11Kneeboard();
  virtual ~OculusD3D11Kneeboard();

  virtual void Unhook() override;

 protected:
  virtual HRESULT OnIDXGISwapChain_Present(
    UINT syncInterval,
    UINT flags,
    IDXGISwapChain* swapChain,
    const decltype(&IDXGISwapChain::Present)& next) override;

  virtual ovrTextureSwapChain GetSwapChain(
    ovrSession session,
    const SHM::Header& config) override final;
  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot) override final;

 private:
  class Impl;
  std::unique_ptr<Impl> p;
};
}// namespace OpenKneeboard
