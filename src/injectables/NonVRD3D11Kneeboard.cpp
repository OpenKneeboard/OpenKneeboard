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

#include <DirectXTK/SpriteBatch.h>
#include <OpenKneeboard/dprint.h>
#include <dxgi.h>
#include <winrt/base.h>

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
  auto shm = mSHM.MaybeGet();
  if (!shm) {
    return std::invoke(next, swapChain, syncInterval, flags);
  }

  const auto& config = *shm.GetConfig();

  winrt::com_ptr<ID3D11Device> device;
  swapChain->GetDevice(IID_PPV_ARGS(device.put()));
  if (device.get() != mDevice) {
    mDevice = device.get();
    mDeviceResources = {};
    device->GetImmediateContext(mDeviceResources.mContext.put());
    mDeviceResources.mSpriteBatch
      = std::make_unique<DirectX::SpriteBatch>(mDeviceResources.mContext.get());
  }

  if (mDeviceResources.mLastSequenceNumber != shm.GetSequenceNumber()) {
    mDeviceResources.mTexture = nullptr;
    mDeviceResources.mResourceView = nullptr;

    static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
    auto textureName = SHM::SharedTextureName();
    device.as<ID3D11Device1>()->OpenSharedResourceByName(
      textureName.c_str(),
      DXGI_SHARED_RESOURCE_READ,
      IID_PPV_ARGS(mDeviceResources.mTexture.put()));
    device->CreateShaderResourceView(
      mDeviceResources.mTexture.get(),
      nullptr,
      mDeviceResources.mResourceView.put());
  }

  DXGI_SWAP_CHAIN_DESC scDesc;
  swapChain->GetDesc(&scDesc);

  const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
  const LONG canvasWidth = scDesc.BufferDesc.Width;
  const LONG canvasHeight = scDesc.BufferDesc.Height;

  const LONG renderHeight
    = (static_cast<long>(canvasHeight) * config.flat.heightPercent) / 100;
  const LONG renderWidth = std::lround(renderHeight * aspectRatio);

  const auto padding = config.flat.paddingPixels;

  LONG left = padding;
  switch (config.flat.horizontalAlignment) {
    case SHM::FlatConfig::HALIGN_LEFT:
      break;
    case SHM::FlatConfig::HALIGN_CENTER:
      left = (canvasWidth - renderWidth) / 2;
      break;
    case SHM::FlatConfig::HALIGN_RIGHT:
      left = canvasWidth - (renderWidth + padding);
      break;
  }

  LONG top = padding;
  switch (config.flat.verticalAlignment) {
    case SHM::FlatConfig::VALIGN_TOP:
      break;
    case SHM::FlatConfig::VALIGN_MIDDLE:
      top = (canvasHeight - renderHeight) / 2;
      break;
    case SHM::FlatConfig::VALIGN_BOTTOM:
      top = canvasHeight - (renderHeight + padding);
      break;
  }

  auto& sprites = *mDeviceResources.mSpriteBatch;
  RECT destRect {left, top, left + renderWidth, top + renderHeight};
  RECT sourceRect {
    0,
    0,
    config.imageWidth,
    config.imageHeight,
  };
  sprites.Begin();
  sprites.Draw(
    mDeviceResources.mResourceView.get(),
    destRect,
    &sourceRect,
    DirectX::Colors::White * config.flat.opacity);
  sprites.End();

  return std::invoke(next, swapChain, syncInterval, flags);
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
