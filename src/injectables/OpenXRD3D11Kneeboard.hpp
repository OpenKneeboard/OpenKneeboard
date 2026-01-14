// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "OpenXRKneeboard.hpp"

#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/D3D11/Renderer.hpp>
#include <OpenKneeboard/SHM/D3D11.hpp>

#include <OpenKneeboard/config.hpp>

struct XrGraphicsBindingD3D11KHR;

namespace OpenKneeboard {

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(
    XrInstance,
    XrSystemId,
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingD3D11KHR&);
  ~OpenXRD3D11Kneeboard();

  struct DXGIFormats {
    DXGI_FORMAT mTextureFormat;
    DXGI_FORMAT mRenderTargetViewFormat;
  };
  static DXGIFormats GetDXGIFormats(OpenXRNext*, XrSession);

 protected:
  SHM::Reader& GetSHM() override;
  XrSwapchain CreateSwapchain(XrSession, const PixelSize&) override;
  void ReleaseSwapchainResources(XrSwapchain) override;

  void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    SHM::Frame,
    const std::span<SHM::LayerSprite>& layers) override;

 private:
  std::unique_ptr<SHM::D3D11::Reader> mSHM;

  winrt::com_ptr<ID3D11Device> mDevice;
  winrt::com_ptr<ID3D11DeviceContext1> mImmediateContext;
  std::unique_ptr<D3D11::Renderer> mRenderer;

  D3D11::DeviceContextState mRenderState;

  using SwapchainBufferResources = D3D11::SwapchainBufferResources;
  using SwapchainResources = D3D11::SwapchainResources;

  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;
};

}// namespace OpenKneeboard
