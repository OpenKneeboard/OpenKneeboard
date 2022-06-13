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
#include <shims/winrt.h>

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
  if (device.get() != mDevice) {
    mDevice = device.get();
    mDeviceResources = {};
    device->GetImmediateContext(mDeviceResources.mContext.put());
    mDeviceResources.mSpriteBatch
      = std::make_unique<DirectX::SpriteBatch>(mDeviceResources.mContext.get());
  }

  auto snapshot = mSHM.MaybeGet();
  if (!snapshot.IsValid()) {
    return passthrough();
  }

  const auto config = snapshot.GetConfig();
  const auto& layer = snapshot.GetLayers()[0];

  DXGI_SWAP_CHAIN_DESC scDesc;
  swapChain->GetDesc(&scDesc);

  const auto aspectRatio = float(layer.mImageWidth) / layer.mImageHeight;
  const LONG canvasWidth = scDesc.BufferDesc.Width;
  const LONG canvasHeight = scDesc.BufferDesc.Height;

  const LONG renderHeight
    = (static_cast<long>(canvasHeight) * config.mFlat.heightPercent) / 100;
  const LONG renderWidth = std::lround(renderHeight * aspectRatio);

  const auto padding = config.mFlat.paddingPixels;

  LONG left = padding;
  switch (config.mFlat.horizontalAlignment) {
    case FlatConfig::HALIGN_LEFT:
      break;
    case FlatConfig::HALIGN_CENTER:
      left = (canvasWidth - renderWidth) / 2;
      break;
    case FlatConfig::HALIGN_RIGHT:
      left = canvasWidth - (renderWidth + padding);
      break;
  }

  LONG top = padding;
  switch (config.mFlat.verticalAlignment) {
    case FlatConfig::VALIGN_TOP:
      break;
    case FlatConfig::VALIGN_MIDDLE:
      top = (canvasHeight - renderHeight) / 2;
      break;
    case FlatConfig::VALIGN_BOTTOM:
      top = canvasHeight - (renderHeight + padding);
      break;
  }

  auto& sprites = *mDeviceResources.mSpriteBatch;
  RECT destRect {left, top, left + renderWidth, top + renderHeight};
  RECT sourceRect {
    0,
    0,
    layer.mImageWidth,
    layer.mImageHeight,
  };

  auto sharedTexture = snapshot.GetSharedTexture(mDevice);
  if (!sharedTexture) {
    return passthrough();
  }
  winrt::com_ptr<ID3D11ShaderResourceView> resourceView;
  mDevice->CreateShaderResourceView(
    sharedTexture.GetTexture(), nullptr, resourceView.put());

  sprites.Begin();
  sprites.Draw(
    resourceView.get(),
    destRect,
    &sourceRect,
    DirectX::Colors::White * config.mFlat.opacity);
  sprites.End();
  mDeviceResources.mContext->Flush();

  return passthrough();
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
