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
#include "NonVRD3D11Kneeboard.h"

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <dxgi.h>
#include <shims/winrt/base.h>

#include "InjectedDLLMain.h"

namespace OpenKneeboard {

NonVRD3D11Kneeboard::NonVRD3D11Kneeboard() {
  mDXGIHook.InstallHook({
    .onPresent
    = std::bind_front(&NonVRD3D11Kneeboard::OnIDXGISwapChain_Present, this),
  });
}

NonVRD3D11Kneeboard::~NonVRD3D11Kneeboard() {
  this->UninstallHook();
}

void NonVRD3D11Kneeboard::UninstallHook() {
  mDXGIHook.UninstallHook();
}

HRESULT NonVRD3D11Kneeboard::OnIDXGISwapChain_Present(
  IDXGISwapChain* swapChain,
  UINT syncInterval,
  UINT flags,
  const decltype(&IDXGISwapChain::Present)& next) {
  auto passthrough = std::bind_front(next, swapChain, syncInterval, flags);
  if (!mSHM) {
    return passthrough();
  }

  winrt::com_ptr<ID3D11Device> device;
  swapChain->GetDevice(IID_PPV_ARGS(device.put()));

  this->CreateTexture(device);

  if (mSHM.GetRenderCacheKey() != mCacheKey) {
    auto snapshot = mSHM.MaybeGet(SHM::ConsumerKind::NonVRD3D11);
    if (!snapshot.IsValid()) {
      mHaveLayer = false;
      mCacheKey = snapshot.GetRenderCacheKey();
      return passthrough();
    }

    if (snapshot.GetLayerCount() == 0) {
      mHaveLayer = false;
      mCacheKey = snapshot.GetRenderCacheKey();
      return passthrough();
    }

    auto config = snapshot.GetConfig();
    const uint8_t layerIndex = 0;
    const auto& layer = *snapshot.GetLayerConfig(layerIndex);

    winrt::com_ptr<ID3D11DeviceContext> ctx;
    device->GetImmediateContext(ctx.put());
    ctx->CopyResource(
      mTexture.get(),
      snapshot.GetLayerTexture(device.get(), layerIndex).GetTexture());

    mHaveLayer = true;
    mCacheKey = snapshot.GetRenderCacheKey();
    mFlatConfig = config.mFlat;
    mLayerConfig = layer;
  }

  if (!mHaveLayer) {
    return passthrough();
  }

  DXGI_SWAP_CHAIN_DESC scDesc;
  swapChain->GetDesc(&scDesc);

  const auto aspectRatio
    = float(mLayerConfig.mImageWidth) / mLayerConfig.mImageHeight;
  const LONG canvasWidth = scDesc.BufferDesc.Width;
  const LONG canvasHeight = scDesc.BufferDesc.Height;

  const LONG renderHeight
    = (static_cast<long>(canvasHeight) * mFlatConfig.mHeightPercent) / 100;
  const LONG renderWidth = std::lround(renderHeight * aspectRatio);

  const auto padding = mFlatConfig.mPaddingPixels;

  LONG left = padding;
  switch (mFlatConfig.mHorizontalAlignment) {
    case FlatConfig::HorizontalAlignment::Left:
      break;
    case FlatConfig::HorizontalAlignment::Center:
      left = (canvasWidth - renderWidth) / 2;
      break;
    case FlatConfig::HorizontalAlignment::Right:
      left = canvasWidth - (renderWidth + padding);
      break;
  }

  LONG top = padding;
  switch (mFlatConfig.mVerticalAlignment) {
    case FlatConfig::VerticalAlignment::Top:
      break;
    case FlatConfig::VerticalAlignment::Middle:
      top = (canvasHeight - renderHeight) / 2;
      break;
    case FlatConfig::VerticalAlignment::Bottom:
      top = canvasHeight - (renderHeight + padding);
      break;
  }

  RECT destRect {left, top, left + renderWidth, top + renderHeight};
  RECT sourceRect {
    0,
    0,
    mLayerConfig.mImageWidth,
    mLayerConfig.mImageHeight,
  };

  {
    winrt::com_ptr<ID3D11Texture2D> destinationTexture;
    swapChain->GetBuffer(0, IID_PPV_ARGS(destinationTexture.put()));
    if (!destinationTexture) {
      OPENKNEEBOARD_BREAK;
      return passthrough();
    }

    winrt::com_ptr<ID3D11RenderTargetView> rtv;
    auto result = device->CreateRenderTargetView(
      destinationTexture.get(), nullptr, rtv.put());
    if (!rtv) {
      dprintf(
        "Failed to create RenderTargetView: {} ({:#08x})",
        result,
        std::bit_cast<uint32_t>(result));
      OPENKNEEBOARD_BREAK;
      return passthrough();
    }

    D3D11::DrawTextureWithOpacity(
      device.get(),
      mShaderResourceView.get(),
      rtv.get(),
      sourceRect,
      destRect,
      mFlatConfig.mOpacity);
  }

  return passthrough();
}

void NonVRD3D11Kneeboard::CreateTexture(
  const winrt::com_ptr<ID3D11Device>& device) {
  if (mTexture) [[likely]] {
    return;
  }

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

  winrt::check_hresult(
    device->CreateTexture2D(&textureDesc, nullptr, mTexture.put()));
  winrt::check_hresult(device->CreateShaderResourceView(
    mTexture.get(), nullptr, mShaderResourceView.put()));
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {
std::unique_ptr<NonVRD3D11Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<NonVRD3D11Kneeboard>();
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-D3D11", gInstance, &ThreadEntry, hinst, dwReason, reserved);
}
