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
  const D3D11On12DeviceResources& deviceResources,
  const winrt::com_ptr<ID3D12Resource>& texture12,
  const winrt::com_ptr<ID3D11Texture2D>& texture11,
  const winrt::com_ptr<ID3D11Texture2D>& bufferTexture11,
  const winrt::com_ptr<ID3D11RenderTargetView>& renderTargetView)
  : mDeviceResources(deviceResources),
    mTexture12(texture12),
    mTexture11(texture11),
    mBufferTexture11(bufferTexture11),
    mRenderTargetView(renderTargetView) {
}

D3D11On12RenderTargetView::~D3D11On12RenderTargetView() {
  if (mBufferTexture11) {
    winrt::com_ptr<ID3D12CommandAllocator> allocator;
    winrt::check_hresult(mDeviceResources.mDevice12->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.put())));
    winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
    winrt::check_hresult(mDeviceResources.mDevice12->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      allocator.get(),
      nullptr,
      IID_PPV_ARGS(commandList.put())));
    winrt::com_ptr<ID3D12Resource> bufferTexture12;
    winrt::check_hresult(mDeviceResources.m11on12.as<ID3D11On12Device2>()
                           ->UnwrapUnderlyingResource(
                             mBufferTexture11.get(),
                             mDeviceResources.mCommandQueue12.get(),
                             IID_PPV_ARGS(bufferTexture12.put())));

    D3D12_RESOURCE_BARRIER inBarrier {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = {
        mTexture12.get(),
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_DEST,
      },
    };
    commandList->ResourceBarrier(1, &inBarrier);
    commandList->CopyResource(mTexture12.get(), bufferTexture12.get());

    D3D12_RESOURCE_BARRIER outBarrier {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = {
        mTexture12.get(),
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
      },
    };

    commandList->ResourceBarrier(1, &outBarrier);
    winrt::check_hresult(commandList->Close());

    const auto commandLists
      = reinterpret_cast<ID3D12CommandList*>(commandList.get());
    mDeviceResources.mCommandQueue12->ExecuteCommandLists(1, &commandLists);

    winrt::check_hresult(mDeviceResources.m11on12.as<ID3D11On12Device2>()
                           ->ReturnUnderlyingResource(
                             mBufferTexture11.get(), 0, nullptr, nullptr));
  }
}

ID3D11RenderTargetView* D3D11On12RenderTargetView::Get() const {
  return mRenderTargetView.get();
}

D3D11On12RenderTargetViewFactory::D3D11On12RenderTargetViewFactory(
  const D3D11On12DeviceResources& deviceResources,
  const winrt::com_ptr<ID3D12Resource>& texture12)
  : mDeviceResources(deviceResources), mTexture12(texture12) {
  D3D11_RESOURCE_FLAGS resourceFlags11 {};
  winrt::check_hresult(deviceResources.m11on12->CreateWrappedResource(
    texture12.get(),
    &resourceFlags11,
    D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_STATE_COMMON,
    IID_PPV_ARGS(mTexture11.put())));

  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = TextureWidth,
    .Height = TextureHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags = 0,
  };
  winrt::check_hresult(deviceResources.mDevice11->CreateTexture2D(
    &textureDesc, nullptr, mBufferTexture11.put()));

  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };
  winrt::check_hresult(deviceResources.mDevice11->CreateRenderTargetView(
    mBufferTexture11.get(), &rtvd, mRenderTargetView.put()));
}

D3D11On12RenderTargetViewFactory::~D3D11On12RenderTargetViewFactory() = default;

std::unique_ptr<RenderTargetView> D3D11On12RenderTargetViewFactory::Get()
  const {
  return std::make_unique<D3D11On12RenderTargetView>(
    mDeviceResources,
    mTexture12,
    mTexture11,
    mBufferTexture11,
    mRenderTargetView);
}

}// namespace OpenKneeboard::D3D11
