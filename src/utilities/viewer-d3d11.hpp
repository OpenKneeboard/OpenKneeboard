// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "viewer.hpp"

#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/SHM/D3D11.hpp>

#include <shims/winrt/base.h>

#include <d3d11.h>

namespace OpenKneeboard::Viewer {

class D3D11Renderer final : public Renderer {
 public:
  D3D11Renderer() = delete;
  D3D11Renderer(const winrt::com_ptr<ID3D11Device>&);
  virtual ~D3D11Renderer();
  virtual std::wstring_view GetName() const noexcept override;

  virtual SHM::CachedReader* GetSHM() override;

  virtual void Initialize(uint8_t swapchainLength) override;

  virtual void SaveTextureToFile(
    SHM::IPCClientTexture*,
    const std::filesystem::path&) override;

  virtual uint64_t Render(
    SHM::IPCClientTexture* sourceTexture,
    const PixelRect& sourceRect,
    HANDLE destTexture,
    const PixelSize& destTextureDimensions,
    const PixelRect& destRect,
    HANDLE fence,
    uint64_t fenceValueIn) override;

 private:
  SHM::D3D11::CachedReader mSHM {SHM::ConsumerKind::Viewer};

  uint64_t mSessionID {};

  winrt::com_ptr<ID3D11Device1> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext> mD3D11ImmediateContext;

  std::unique_ptr<OpenKneeboard::D3D11::SpriteBatch> mSpriteBatch;

  PixelSize mDestDimensions;
  HANDLE mDestHandle {};
  winrt::com_ptr<ID3D11Texture2D> mDestTexture;
  winrt::com_ptr<ID3D11RenderTargetView> mDestRenderTargetView;
};

}// namespace OpenKneeboard::Viewer