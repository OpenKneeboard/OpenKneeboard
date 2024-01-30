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
#include <OpenKneeboard/D3D11/Renderer.h>

#include <OpenKneeboard/hresult.h>

namespace OpenKneeboard::D3D11 {

SwapchainBufferResources::SwapchainBufferResources(
  ID3D11Device* device,
  ID3D11Texture2D* texture,
  DXGI_FORMAT renderTargetViewFormat) {
  mTexture = texture;

  D3D11_TEXTURE2D_DESC textureDesc;
  texture->GetDesc(&textureDesc);

  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc {
    .Format = renderTargetViewFormat,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {0},
  };
  check_hresult(
    device->CreateRenderTargetView(texture, &rtvDesc, mRenderTargetView.put()));
}

Renderer::Renderer(ID3D11Device* device) {
  mSpriteBatch = std::make_unique<SpriteBatch>(device);
}

void Renderer::RenderLayers(
  const SwapchainResources& sr,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  const std::span<SHM::LayerRenderInfo>& layers,
  RenderMode renderMode) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::Renderer::RenderLayers()");

  auto source
    = snapshot.GetTexture<SHM::D3D11::Texture>()->GetD3D11ShaderResourceView();

  const auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  auto dest = br.mRenderTargetView.get();

  mSpriteBatch->Begin(dest, sr.mDimensions);

  if (renderMode == RenderMode::ClearAndRender) {
    mSpriteBatch->Clear();
  }

  for (const auto& layer: layers) {
    const auto sourceRect
      = snapshot.GetLayerConfig(layer.mLayerIndex)->mLocationOnTexture;
    const auto& destRect = layer.mDestRect;
    const D3D11::Opacity opacity {layer.mOpacity};
    mSpriteBatch->Draw(source, sourceRect, destRect, opacity);
  }
  mSpriteBatch->End();
}

}// namespace OpenKneeboard::D3D11