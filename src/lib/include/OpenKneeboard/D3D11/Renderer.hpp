// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/RenderMode.hpp>
#include <OpenKneeboard/SHM/D3D11.hpp>

#include <span>

namespace OpenKneeboard::D3D11 {

struct SwapchainBufferResources {
  SwapchainBufferResources() = delete;
  SwapchainBufferResources(
    ID3D11Device*,
    ID3D11Texture2D*,
    DXGI_FORMAT renderTargetViewFormat);

  ID3D11Texture2D* mTexture {nullptr};
  winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
};

struct SwapchainResources {
  PixelSize mDimensions;
  std::vector<SwapchainBufferResources> mBufferResources;
};

class Renderer {
 public:
  Renderer() = delete;
  explicit Renderer(ID3D11Device*);

  void RenderLayers(
    const SwapchainResources&,
    uint32_t swapchainTextureIndex,
    const SHM::D3D11::Frame& frame,
    const std::span<SHM::LayerSprite>& layers,
    RenderMode);

 private:
  std::unique_ptr<SpriteBatch> mSpriteBatch;
  wil::com_ptr<ID3D11DeviceContext4> mContext;
};

}// namespace OpenKneeboard::D3D11