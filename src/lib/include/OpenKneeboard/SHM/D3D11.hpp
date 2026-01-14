// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/SHM.hpp>

#include <wil/com.h>

#include <d3d11_4.h>

namespace OpenKneeboard::SHM::D3D11 {

struct Frame {
  using Error = SHM::Frame::Error;
  Config mConfig {};
  decltype(SHM::Frame::mLayers) mLayers {};
  ID3D11Texture2D* mTexture {};
  ID3D11ShaderResourceView* mShaderResourceView {};
  ID3D11Fence* mFence {};
  uint64_t mFenceIn {};
};

class Reader final : public SHM::Reader {
 public:
  Reader(ConsumerKind, ID3D11Device*);
  ~Reader() override;

  Frame Map(SHM::Frame);
  std::expected<Frame, Frame::Error> MaybeGetMapped();

 protected:
  void OnSessionChanged() override;

 private:
  struct FrameD3D11Resources {
    HANDLE mTextureHandle {};
    wil::com_ptr<ID3D11Texture2D> mTexture;
    wil::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;

    HANDLE mFenceHandle {};
    wil::com_ptr<ID3D11Fence> mFence;
  };

  wil::com_ptr<ID3D11Device5> mDevice;
  // wil::com_ptr<ID3D11DeviceContext4> mContext; ?

  std::array<FrameD3D11Resources, SHM::SwapChainLength> mFrames {};
};

}// namespace OpenKneeboard::SHM::D3D11