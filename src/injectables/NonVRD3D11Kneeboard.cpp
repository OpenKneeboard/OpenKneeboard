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
#include <OpenKneeboard/tracing.h>
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

  const auto snapshot
    = mSHM.MaybeGet(device.get(), SHM::ConsumerKind::NonVRD3D11);

  if (!snapshot.GetLayerCount()) {
    return passthrough();
  }

  DXGI_SWAP_CHAIN_DESC scDesc;
  swapChain->GetDesc(&scDesc);

  const auto& layerConfig = *snapshot.GetLayerConfig(0);
  const auto flatConfig = snapshot.GetConfig().mFlat;

  const auto aspectRatio
    = float(layerConfig.mImageWidth) / layerConfig.mImageHeight;
  const LONG canvasWidth = scDesc.BufferDesc.Width;
  const LONG canvasHeight = scDesc.BufferDesc.Height;

  const LONG renderHeight
    = (static_cast<long>(canvasHeight) * flatConfig.mHeightPercent) / 100;
  const LONG renderWidth = std::lround(renderHeight * aspectRatio);

  const auto padding = flatConfig.mPaddingPixels;

  LONG left = padding;
  switch (flatConfig.mHorizontalAlignment) {
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
  switch (flatConfig.mVerticalAlignment) {
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
    layerConfig.mImageWidth,
    layerConfig.mImageHeight,
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

    const auto srv = snapshot.GetLayerShaderResourceView(device.get(), 0);
    if (!srv) {
      dprint("Failed to get layer SRV");
      OPENKNEEBOARD_BREAK;
      return passthrough();
    }

    D3D11::DrawTextureWithOpacity(
      device.get(),
      srv.get(),
      rtv.get(),
      sourceRect,
      destRect,
      flatConfig.mOpacity);
  }

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

namespace OpenKneeboard {
/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.NonVR.D3D11")
 * d301dc28-a6f4-5054-6786-5cdb46c0f270
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.NonVR.D3D11",
  (0xd301dc28, 0xa6f4, 0x5054, 0x67, 0x86, 0x5c, 0xdb, 0x46, 0xc0, 0xf2, 0x70));
}// namespace OpenKneeboard

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-D3D11", gInstance, &ThreadEntry, hinst, dwReason, reserved);
}
