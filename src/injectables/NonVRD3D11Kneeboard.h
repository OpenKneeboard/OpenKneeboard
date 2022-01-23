#pragma once

#include <DirectXTK/SpriteBatch.h>
#include <OpenKneeboard/SHM.h>
#include <winrt/base.h>

#include <memory>

#include "IDXGISwapChainPresentHook.h"

namespace OpenKneeboard {

class NonVRD3D11Kneeboard final {
 public:
  NonVRD3D11Kneeboard();
  virtual ~NonVRD3D11Kneeboard();

  void UninstallHook();

 private:
  SHM::Reader mSHM;
  IDXGISwapChainPresentHook mDXGIHook;
  ID3D11Device* mDevice = nullptr;
  struct {
    winrt::com_ptr<ID3D11DeviceContext> mContext;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;

    winrt::com_ptr<ID3D11Texture2D> mTexture;
    winrt::com_ptr<ID3D11ShaderResourceView> mResourceView;

    uint64_t mLastSequenceNumber;
  } mDeviceResources;

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next);
};

}// namespace OpenKneeboard
