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
#include <DirectXTK/SpriteBatch.h>
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <shims/winrt/base.h>

namespace OpenKneeboard::D3D11 {

void CopyTextureWithTint(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  DirectX::FXMVECTOR tint) {
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());
  ctx->ClearRenderTargetView(dest, DirectX::Colors::Transparent);
  D3D11_VIEWPORT viewport {
    0.0f,
    0.0f,
    static_cast<FLOAT>(TextureWidth),
    static_cast<FLOAT>(TextureHeight),
    0.0f,
    1.0f,
  };
  ctx->RSSetViewports(1, &viewport);
  ctx->OMSetRenderTargets(1, &dest, nullptr);

  DirectX::SpriteBatch sprites(ctx.get());
  sprites.Begin();
  sprites.Draw(source, DirectX::XMFLOAT2 {0.0f, 0.0f}, tint);
  sprites.End();
}

void DrawTextureWithTint(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  const RECT& sourceRect,
  const RECT& destRect,
  DirectX::FXMVECTOR tint) {
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());
  D3D11_VIEWPORT viewport {
    0.0f,
    0.0f,
    static_cast<FLOAT>(destRect.right),
    static_cast<FLOAT>(destRect.bottom),
    0.0f,
    1.0f,
  };
  ctx->RSSetViewports(1, &viewport);
  ctx->OMSetRenderTargets(1, &dest, nullptr);

  DirectX::SpriteBatch sprites(ctx.get());
  sprites.Begin();
  sprites.Draw(source, destRect, &sourceRect, tint);
  sprites.End();
}

void CopyTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  float opacity) {
  CopyTextureWithTint(device, source, dest, DirectX::Colors::White * opacity);
}

void DrawTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  const RECT& sourceRect,
  const RECT& destRect,
  float opacity) {
  DrawTextureWithTint(
    device,
    source,
    dest,
    sourceRect,
    destRect,
    DirectX::Colors::White * opacity);
}

IRenderTargetView::IRenderTargetView() = default;
IRenderTargetView::~IRenderTargetView() = default;
IRenderTargetViewFactory::~IRenderTargetViewFactory() = default;

RenderTargetView::RenderTargetView(
  const winrt::com_ptr<ID3D11RenderTargetView>& impl)
  : mImpl(impl) {
}

RenderTargetView::~RenderTargetView() = default;

ID3D11RenderTargetView* RenderTargetView::Get() const {
  return mImpl.get();
}

RenderTargetViewFactory::RenderTargetViewFactory(
  ID3D11Device* device,
  ID3D11Texture2D* texture) {
  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };

  winrt::check_hresult(
    device->CreateRenderTargetView(texture, &rtvd, mImpl.put()));
}

RenderTargetViewFactory::~RenderTargetViewFactory() = default;

std::unique_ptr<IRenderTargetView> RenderTargetViewFactory::Get() const {
  return std::make_unique<RenderTargetView>(mImpl);
}

}// namespace OpenKneeboard::D3D11
