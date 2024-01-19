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
#include <OpenKneeboard/SHM/D3D11.h>

#include <OpenKneeboard/scope_guard.h>

namespace OpenKneeboard::SHM::D3D11 {

LayerTextureCache::~LayerTextureCache() = default;

ID3D11ShaderResourceView* LayerTextureCache::GetD3D11ShaderResourceView() {
  if (!mD3D11ShaderResourceView) [[unlikely]] {
    auto texture = this->GetD3D11Texture();
    winrt::com_ptr<ID3D11Device> device;
    texture->GetDevice(device.put());
    winrt::check_hresult(device->CreateShaderResourceView(
      texture, nullptr, mD3D11ShaderResourceView.put()));
  }
  return mD3D11ShaderResourceView.get();
}

std::shared_ptr<SHM::LayerTextureCache> CachedReader::CreateLayerTextureCache(
  [[maybe_unused]] uint8_t layerIndex,
  const winrt::com_ptr<ID3D11Texture2D>& texture) {
  return std::make_shared<SHM::D3D11::LayerTextureCache>(texture);
}

namespace Renderer {
DeviceResources::DeviceResources(ID3D11Device* device) {
  mD3D11Device.copy_from(device);
  device->GetImmediateContext(mD3D11ImmediateContext.put());
  mDXTKSpriteBatch
    = std::make_unique<DirectX::SpriteBatch>(mD3D11ImmediateContext.get());
}

SwapchainResources::SwapchainResources(
  DeviceResources* dr,
  DXGI_FORMAT renderTargetViewFormat,
  size_t textureCount,
  ID3D11Texture2D** textures) {
  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc {
    .Format = renderTargetViewFormat,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };

  mBufferResources.reserve(textureCount);

  for (size_t i = 0; i < textureCount; ++i) {
    auto br = std::make_unique<BufferResources>();

    auto texture = textures[i];
    br->m3D11Texture.copy_from(texture);
    winrt::check_hresult(dr->mD3D11Device->CreateRenderTargetView(
      texture, &rtvDesc, br->mD3D11RenderTargetView.put()));

    mBufferResources.push_back(std::move(br));
  }

  D3D11_TEXTURE2D_DESC colorDesc {};
  textures[0]->GetDesc(&colorDesc);

  mViewport = {
    0,
    0,
    static_cast<FLOAT>(colorDesc.Width),
    static_cast<FLOAT>(colorDesc.Height),
    0.0f,
    1.0f,
  };
}

void BeginFrame(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "OpenKneeboard::SHM::D3D11::Renderer::BeginFrame()");

  auto& br = sr->mBufferResources.at(swapchainTextureIndex);

  auto ctx = dr->mD3D11ImmediateContext.get();
  auto rtv = br->mD3D11RenderTargetView.get();

  ctx->RSSetViewports(1, &sr->mViewport);
  ctx->OMSetDepthStencilState(nullptr, 0);
  ctx->OMSetRenderTargets(1, &rtv, nullptr);
  ctx->OMSetBlendState(nullptr, nullptr, ~static_cast<UINT>(0));
  ctx->IASetInputLayout(nullptr);
  ctx->VSSetShader(nullptr, nullptr, 0);

  TraceLoggingWriteStop(
    activity, "OpenKneeboard::SHM::D3D11::Renderer::BeginFrame()");
}

void ClearRenderTargetView(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex) {
  auto rtv = sr->mBufferResources.at(swapchainTextureIndex)
               ->mD3D11RenderTargetView.get();
  dr->mD3D11ImmediateContext->ClearRenderTargetView(
    rtv, DirectX::Colors::Transparent);
}

void Render(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex,
  const SHM::D3D11::CachedReader& shm,
  const SHM::Snapshot& snapshot,
  size_t layerSpriteCount,
  LayerSprite* layerSprites) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "OpenKneeboard::SHM::D3D11::Renderer::Render()");

  auto ctx = dr->mD3D11ImmediateContext.get();
  auto sprites = dr->mDXTKSpriteBatch.get();

  {
    TraceLoggingThreadActivity<gTraceProvider> spritesActivity;
    TraceLoggingWriteStart(spritesActivity, "SpriteBatch");
    sprites->Begin();
    const scope_guard endSprites([&sprites, &spritesActivity]() {
      sprites->End();
      TraceLoggingWriteStop(spritesActivity, "SpriteBatch");
    });

    for (size_t i = 0; i < layerSpriteCount; ++i) {
      const auto& sprite = layerSprites[i];
      auto resources
        = snapshot.GetLayerGPUResources<SHM::D3D11::LayerTextureCache>(
          sprite.mLayerIndex);

      const auto srv = resources->GetD3D11ShaderResourceView();

      if (!srv) {
        dprint("Failed to get shader resource view");
        TraceLoggingWriteStop(
          activity,
          "OpenKneeboard::SHM::D3D11::Renderer::Render()",
          TraceLoggingValue(false, "Success"));
        return;
      }

      auto config = snapshot.GetLayerConfig(sprite.mLayerIndex);

      const D3D11_RECT sourceRect = config->mLocationOnTexture;
      const D3D11_RECT destRect = sprite.mDestRect;

      const auto opacity = sprite.mOpacity;
      DirectX::FXMVECTOR tint {opacity, opacity, opacity, opacity};

      sprites->Draw(srv, destRect, &sourceRect, tint);
    }
  }

  TraceLoggingWriteStop(
    activity,
    "OpenKneeboard::SHM::D3D11::Renderer::Render()",
    TraceLoggingValue(true, "Success"));
}

void EndFrame(
  DeviceResources*,
  SwapchainResources*,
  [[maybe_unused]] uint8_t swapchainTextureIndex) {
}

}// namespace Renderer

}// namespace OpenKneeboard::SHM::D3D11