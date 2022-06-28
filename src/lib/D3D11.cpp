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
#include <OpenKneeboard/config.h>
#include <shims/winrt.h>

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

void CopyTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  float opacity) {
  CopyTextureWithTint(device, source, dest, DirectX::Colors::White * opacity);
}

RenderTargetView::RenderTargetView() = default;

RenderTargetView::~RenderTargetView() = default;

RenderTargetViewFactory::~RenderTargetViewFactory() = default;

D3D11RenderTargetView::D3D11RenderTargetView(
  const winrt::com_ptr<ID3D11RenderTargetView>& impl)
  : mImpl(impl) {
}

D3D11RenderTargetView::~D3D11RenderTargetView() = default;

ID3D11RenderTargetView* D3D11RenderTargetView::Get() const {
  return mImpl.get();
}

D3D11RenderTargetViewFactory::D3D11RenderTargetViewFactory(
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

D3D11RenderTargetViewFactory::~D3D11RenderTargetViewFactory() = default;

std::unique_ptr<RenderTargetView> D3D11RenderTargetViewFactory::Get() const {
  return std::make_unique<D3D11RenderTargetView>(mImpl);
}

D3D11On12RenderTargetView::D3D11On12RenderTargetView(
  const winrt::com_ptr<ID3D11On12Device>& d3d11On12,
  const winrt::com_ptr<ID3D11Texture2D>& texture11,
  const winrt::com_ptr<ID3D11RenderTargetView>& renderTargetView)
  : mD3D11On12(d3d11On12),
    mTexture11(texture11),
    mRenderTargetView(renderTargetView) {
  ID3D11Resource* ptr = mTexture11.get();
  mD3D11On12->AcquireWrappedResources(&ptr, 1);
}

D3D11On12RenderTargetView::~D3D11On12RenderTargetView() {
  ID3D11Resource* ptr = mTexture11.get();
  mD3D11On12->ReleaseWrappedResources(&ptr, 1);
}

ID3D11RenderTargetView* D3D11On12RenderTargetView::Get() const {
  return mRenderTargetView.get();
}

D3D11On12RenderTargetViewFactory::D3D11On12RenderTargetViewFactory(
  const winrt::com_ptr<ID3D11Device>& d3d11,
  const winrt::com_ptr<ID3D11On12Device>& d3d11On12,
  const winrt::com_ptr<ID3D12Resource>& texture12)
  : mD3D11On12(d3d11On12) {
  D3D11_RESOURCE_FLAGS resourceFlags11 {D3D11_BIND_RENDER_TARGET};
  winrt::check_hresult(d3d11On12->CreateWrappedResource(
    texture12.get(),
    &resourceFlags11,
    D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_STATE_COMMON,
    IID_PPV_ARGS(mTexture11.put())));

  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };
  winrt::check_hresult(d3d11->CreateRenderTargetView(
    mTexture11.get(), &rtvd, mRenderTargetView.put()));
}

D3D11On12RenderTargetViewFactory::~D3D11On12RenderTargetViewFactory() = default;

std::unique_ptr<RenderTargetView> D3D11On12RenderTargetViewFactory::Get()
  const {
  return std::make_unique<D3D11On12RenderTargetView>(
    mD3D11On12, mTexture11, mRenderTargetView);
}

}// namespace OpenKneeboard::D3D11
