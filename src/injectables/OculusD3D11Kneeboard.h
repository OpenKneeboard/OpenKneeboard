#pragma once

#include <d3d11.h>
#include <winrt/base.h>

#include "IDXGISwapChainPresentHook.h"
#include "OculusKneeboard.h"

namespace OpenKneeboard {

class OculusD3D11Kneeboard final : public OculusKneeboard::Renderer {
 public:
  OculusD3D11Kneeboard();
  virtual ~OculusD3D11Kneeboard();

  void UninstallHook();

 protected:
  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    const SHM::Config& config) override final;

  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot) override final;

 private:
  std::vector<winrt::com_ptr<ID3D11RenderTargetView>> mRenderTargets;
  winrt::com_ptr<ID3D11Device> mD3D = nullptr;

  std::unique_ptr<OculusKneeboard> mOculusKneeboard;
  IDXGISwapChainPresentHook mDXGIHook;

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next);
};
}// namespace OpenKneeboard
