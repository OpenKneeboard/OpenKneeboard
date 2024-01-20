/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <OpenKneeboard/SHM.h>

#include <directxtk/SpriteBatch.h>

namespace OpenKneeboard::SHM::D3D11 {

class TextureProvider : public SHM::TextureProvider {
 public:
  TextureProvider() = delete;
  TextureProvider(const winrt::com_ptr<ID3D11Device>&);
  virtual ~TextureProvider();
  winrt::com_ptr<ID3D11Texture2D> GetD3D11Texture(
    const PixelSize&) noexcept override;
  ID3D11Texture2D* GetD3D11Texture() const noexcept;
  ID3D11ShaderResourceView* GetD3D11ShaderResourceView();

 private:
  PixelSize mPixelSize;
  winrt::com_ptr<ID3D11Device> mD3D11Device;
  winrt::com_ptr<ID3D11Texture2D> mD3D11Texture;
  winrt::com_ptr<ID3D11ShaderResourceView> mD3D11ShaderResourceView;
};

class CachedReader : public SHM::CachedReader {
 public:
  CachedReader() = delete;
  CachedReader(ID3D11Device*, uint8_t swapchainLength);
  virtual ~CachedReader();

 protected:
  virtual std::shared_ptr<SHM::TextureProvider> CreateTextureProvider(
    uint8_t swapchainLength,
    uint8_t swapchainIndex) noexcept override;

  winrt::com_ptr<ID3D11Device> mD3D11Device;
};

// Usage:
// - create DeviceResources and SwapchainResources
// - Call BeginFrame(), Render() [, Render(), ...], EndFrame()
// - Optionally call ClearRenderTargetView() after BeginFrame(), depending on
// your needs.
namespace Renderer {
struct DeviceResources {
  DeviceResources() = delete;
  DeviceResources(ID3D11Device*);

  winrt::com_ptr<ID3D11Device> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext> mD3D11ImmediateContext;

  std::unique_ptr<DirectX::DX11::SpriteBatch> mDXTKSpriteBatch;
};

struct SwapchainResources {
  SwapchainResources() = delete;
  SwapchainResources(
    DeviceResources*,
    DXGI_FORMAT renderTargetViewFormat,
    size_t textureCount,
    ID3D11Texture2D** textures);

  D3D11_VIEWPORT mViewport;

  struct BufferResources {
    winrt::com_ptr<ID3D11Texture2D> m3D11Texture;
    winrt::com_ptr<ID3D11RenderTargetView> mD3D11RenderTargetView;
  };

  std::vector<std::unique_ptr<BufferResources>> mBufferResources;
};

void BeginFrame(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

void ClearRenderTargetView(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

void Render(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex,
  const SHM::D3D11::CachedReader&,
  const SHM::Snapshot&,
  size_t layerSpriteCount,
  LayerSprite* layerSprites);

void EndFrame(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

}// namespace Renderer

};// namespace OpenKneeboard::SHM::D3D11