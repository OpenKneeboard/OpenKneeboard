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

#include "InjectedDLLMain.h"

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <d3d11.h>
#include <dxgi.h>

namespace OpenKneeboard {

NonVRD3D11Kneeboard::NonVRD3D11Kneeboard() {
  mPresentHook.InstallHook({
    .onPresent
    = std::bind_front(&NonVRD3D11Kneeboard::OnIDXGISwapChain_Present, this),
    .onResizeBuffers = std::bind_front(
      &NonVRD3D11Kneeboard::OnIDXGISwapChain_ResizeBuffers, this),
  });
}

NonVRD3D11Kneeboard::~NonVRD3D11Kneeboard() {
  this->UninstallHook();
}

void NonVRD3D11Kneeboard::UninstallHook() {
  mPresentHook.UninstallHook();
}

void NonVRD3D11Kneeboard::InitializeResources(IDXGISwapChain* swapchain) {
  if (mResources && mResources->mSwapchain == swapchain) {
    return;
  }
  OPENKNEEBOARD_TraceLoggingScope("InitializeResources");

  winrt::com_ptr<ID3D11Device> device;
  winrt::check_hresult(swapchain->GetDevice(IID_PPV_ARGS(device.put())));
  winrt::com_ptr<ID3D11DeviceContext> context;
  device->GetImmediateContext(context.put());

  winrt::com_ptr<ID3D11Texture2D> destinationTexture;
  swapchain->GetBuffer(0, IID_PPV_ARGS(destinationTexture.put()));

  if (!destinationTexture) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  D3D11_TEXTURE2D_DESC textureDesc;
  destinationTexture->GetDesc(&textureDesc);

  if (mResources) {
    mSHM.InitializeCache(device.get(), 0);
  }

  mResources = Resources {
    device.get(),
    context.get(),
    swapchain,
    SwapchainResources {
      PixelSize {textureDesc.Width, textureDesc.Height},
      {
        SwapchainBufferResources {
          device.get(),
          destinationTexture.get(),
          textureDesc.Format,
        },
      },
    },
    std::make_unique<D3D11::Renderer>(device.get()),
  };

  mSHM.InitializeCache(device.get(), 1);
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

  OPENKNEEBOARD_TraceLoggingScope(
    "NonVRD3D11Kneeboard::OnIDXGISwapChain_Present");

  this->InitializeResources(swapChain);
  if (!mResources) {
    return passthrough();
  }

  const auto snapshot = mSHM.MaybeGet();
  if (!snapshot.HasTexture()) {
    return passthrough();
  }
  const auto layerCount = snapshot.GetLayerCount();

  if (!layerCount) {
    return passthrough();
  }

  const SHM::LayerConfig* layerConfig = nullptr;
  uint8_t layerIndex = 0;
  for (uint8_t i = 0; i < layerCount; ++i) {
    layerConfig = snapshot.GetLayerConfig(i);
    if (layerConfig->mNonVREnabled) {
      layerIndex = i;
      break;
    }
    layerConfig = nullptr;
  }

  if (!layerConfig) {
    return passthrough();
  }

  const auto& sr = mResources->mSwapchainResources;

  SHM::ActiveConsumers::SetNonVRPixelSize(sr.mDimensions);

  const auto flatConfig = layerConfig->mNonVR;
  const auto& imageSize = layerConfig->mNonVR.mLocationOnTexture.mSize;

  const auto destRect = flatConfig.mPosition.Layout(
    sr.mDimensions, layerConfig->mNonVR.mLocationOnTexture.mSize);

  SHM::LayerSprite layer {
    .mSourceRect = layerConfig->mNonVR.mLocationOnTexture,
    .mDestRect = destRect,
    .mOpacity = layerConfig->mNonVR.mOpacity,
  };

  {
    D3D11::SavedState savedState(mResources->mImmediateContext);
    mResources->mRenderer->RenderLayers(
      sr, 0, snapshot, {&layer, 1}, RenderMode::Overlay);
  }

  return passthrough();
}

HRESULT NonVRD3D11Kneeboard::OnIDXGISwapChain_ResizeBuffers(
  IDXGISwapChain* swapchain,
  UINT bufferCount,
  UINT width,
  UINT height,
  DXGI_FORMAT newFormat,
  UINT swapChainFlags,
  const decltype(&IDXGISwapChain::ResizeBuffers)& next) {
  const auto passthrough = std::bind_front(
    next, swapchain, bufferCount, width, height, newFormat, swapChainFlags);
  if (!mSHM) {
    return passthrough();
  }

  OPENKNEEBOARD_TraceLoggingScope(
    "NonVRD3D11Kneeboard::OnIDXGISwapChain_ResizeBuffers");
  if (!mResources) {
    return passthrough();
  }

  if (mResources->mDevice) {
    mSHM.InitializeCache(mResources->mDevice, 0);
  }

  mResources = {};
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
