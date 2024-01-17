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

class LayerTextureCache : public SHM::LayerTextureCache {
 public:
  using SHM::LayerTextureCache::LayerTextureCache;

  virtual ~LayerTextureCache();
  ID3D11ShaderResourceView* GetD3D11ShaderResourceView();

 private:
  winrt::com_ptr<ID3D11ShaderResourceView> mD3D11ShaderResourceView;
};

class CachedReader : public SHM::CachedReader {
 public:
  using SHM::CachedReader::CachedReader;

 protected:
  virtual std::shared_ptr<SHM::LayerTextureCache> CreateLayerTextureCache(
    uint8_t layerIndex,
    const winrt::com_ptr<ID3D11Texture2D>&) override;
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

  std::vector<winrt::com_ptr<ID3D11Texture2D>> mD3D11Textures;
  std::vector<winrt::com_ptr<ID3D11RenderTargetView>> mD3D11RenderTargetViews;
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