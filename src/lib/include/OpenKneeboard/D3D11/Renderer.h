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

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/RenderMode.h>
#include <OpenKneeboard/SHM/D3D11.h>

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
  Renderer(ID3D11Device*);

  void RenderLayers(
    const SwapchainResources&,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const std::span<SHM::LayerRenderInfo>& layers,
    RenderMode);

 private:
  std::unique_ptr<SpriteBatch> mSpriteBatch;
};

}// namespace OpenKneeboard::D3D11