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

#include "OpenXRD3D11Kneeboard.h"

#include "OpenXRNext.h"

#include <OpenKneeboard/D3D11.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <directxtk/SpriteBatch.h>

#include <d3d11.h>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D11KHR& binding)
  : OpenXRKneeboard(session, runtimeID, next) {
  dprintf("{}", __FUNCTION__);
  TraceLoggingWrite(gTraceProvider, "OpenXRD3D11Kneeboard()");

  mDeviceResources = std::make_unique<DeviceResources>(binding.device);
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRD3D11Kneeboard()");
}

bool OpenXRD3D11Kneeboard::ConfigurationsAreCompatible(
  const VRRenderConfig&,
  const VRRenderConfig&) const {
  return true;
}

OpenXRD3D11Kneeboard::DXGIFormats OpenXRD3D11Kneeboard::GetDXGIFormats(
  OpenXRNext* oxr,
  XrSession session) {
  uint32_t formatCount {0};
  if (XR_FAILED(
        oxr->xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr))) {
    dprint("Failed to get swapchain format count");
    return {};
  }
  std::vector<int64_t> formats;
  formats.resize(formatCount);
  if (
    XR_FAILED(oxr->xrEnumerateSwapchainFormats(
      session, formatCount, &formatCount, formats.data()))
    || formatCount == 0) {
    dprint("Failed to enumerate swapchain formats");
    return {};
  }
  for (const auto it: formats) {
    dprintf("Runtime supports swapchain format: {}", it);
  }
  // If this changes, we probably want to change the preference list below
  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  std::vector<DXGIFormats> preferredFormats {
    {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM},
    {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM},
  };
  for (const auto preferred: preferredFormats) {
    auto it = std::ranges::find(formats, preferred.mTextureFormat);
    if (it != formats.end()) {
      return preferred;
    }
  }

  auto format = static_cast<DXGI_FORMAT>(formats.front());
  return {format, format};
}

XrSwapchain OpenXRD3D11Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size,
  const VRRenderConfig::Quirks&) {
  dprintf("{}", __FUNCTION__);

  auto oxr = this->GetOpenXR();

  auto formats = GetDXGIFormats(oxr, session);
  dprintf(
    "Creating swapchain with format {}",
    static_cast<int>(formats.mTextureFormat));

  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
    .format = formats.mTextureFormat,
    .sampleCount = 1,
    .width = size.mWidth,
    .height = size.mHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  XrSwapchain swapchain {nullptr};

  auto nextResult = oxr->xrCreateSwapchain(session, &swapchainInfo, &swapchain);
  if (XR_FAILED(nextResult)) {
    dprintf("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult
    = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || XR_FAILED(nextResult)) {
    dprintf("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprintf("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D11KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D11KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
    });
  nextResult = oxr->xrEnumerateSwapchainImages(
    swapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (XR_FAILED(nextResult)) {
    dprintf("Failed to enumerate images in swapchain: {}", nextResult);
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
    dprint("Swap chain is not a D3D11 swapchain");
    OPENKNEEBOARD_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  std::vector<ID3D11Texture2D*> textures;
  textures.reserve(imageCount);
  for (size_t i = 0; i < imageCount; ++i) {
    auto& image = images.at(i);
#ifdef DEBUG
    if (image.type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
      OPENKNEEBOARD_BREAK;
    }
#endif
    textures.push_back(image.texture);
  }
  mSwapchainResources[swapchain] = std::make_unique<SwapchainResources>(
    mDeviceResources.get(),
    formats.mRenderTargetViewFormat,
    textures.size(),
    textures.data());

  return swapchain;
}

void OpenXRD3D11Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (mSwapchainResources.contains(swapchain)) {
    mSwapchainResources.erase(swapchain);
  }
}

winrt::com_ptr<ID3D11Device> OpenXRD3D11Kneeboard::GetD3D11Device() {
  if (!mDeviceResources) {
    return {};
  }
  return mDeviceResources->mD3D11Device;
}

bool OpenXRD3D11Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  LayerRenderInfo* layers) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "OpenXRD3D11Kneeboard::RenderLayers()");

  namespace R = SHM::D3D11::Renderer;

  std::vector<R::LayerSprite> sprites;
  sprites.reserve(layerCount);
  for (uint8_t i = 0; i < layerCount; ++i) {
    const auto& layer = layers[i];
    sprites.push_back(R::LayerSprite {
      .mLayerIndex = layer.mLayerIndex,
      .mDestRect = layer.mDestRect,
      .mOpacity = layer.mVR.mKneeboardOpacity,
    });
  }

  auto dr = mDeviceResources.get();
  auto sr = mSwapchainResources.at(swapchain).get();
  R::BeginFrame(dr, sr, swapchainTextureIndex);
  R::ClearRenderTargetView(dr, sr, swapchainTextureIndex);
  R::Render(
    dr,
    sr,
    swapchainTextureIndex,
    mSHM,
    snapshot,
    sprites.size(),
    sprites.data());
  R::EndFrame(dr, sr, swapchainTextureIndex);

  TraceLoggingWriteStop(activity, "OpenXRD3D11Kneeboard::RenderLayers()");
  return true;
}

bool OpenXRD3D11Kneeboard::RenderLayers(
  OpenXRNext* oxr,
  ID3D11Device* device,
  ID3D11RenderTargetView* rtv,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  LayerRenderInfo* layerRenderInfo) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "OpenXRD3D11Kneeboard::RenderLayers()");
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());

  D3D11::SavedState state(ctx);

  D3D11_RENDER_TARGET_VIEW_DESC rtvd;
  rtv->GetDesc(&rtvd);
  D3D11_VIEWPORT viewport {
    0.0f,
    0.0f,
    static_cast<FLOAT>(TextureWidth * MaxLayers),
    static_cast<FLOAT>(TextureHeight),
    0.0f,
    1.0f,
  };
  ctx->RSSetViewports(1, &viewport);
  ctx->OMSetDepthStencilState(nullptr, 0);
  ctx->OMSetRenderTargets(1, &rtv, nullptr);
  ctx->OMSetBlendState(nullptr, nullptr, ~static_cast<UINT>(0));
  ctx->IASetInputLayout(nullptr);
  ctx->VSSetShader(nullptr, nullptr, 0);

  {
    TraceLoggingThreadActivity<gTraceProvider> spritesActivity;
    TraceLoggingWriteStart(spritesActivity, "SpriteBatch");
    DirectX::SpriteBatch sprites(ctx.get());
    sprites.Begin();
    const scope_guard endSprites([&sprites, &spritesActivity]() {
      sprites.End();
      TraceLoggingWriteStop(spritesActivity, "SpriteBatch");
    });

    for (uint8_t i = 0; i < layerCount; ++i) {
      const auto& info = layerRenderInfo[i];
      auto resources
        = snapshot.GetLayerGPUResources<SHM::D3D11::LayerTextureCache>(i);
      const auto srv = resources->GetD3D11ShaderResourceView();
      if (!srv) {
        dprint("Failed to get shader resource view");
        TraceLoggingWriteStop(
          activity,
          "OpenXRD3D11Kneeboard::RenderLayers()",
          TraceLoggingValue(false, "Success"));
        return false;
      }

      const auto opacity = info.mVR.mKneeboardOpacity;
      DirectX::FXMVECTOR tint {opacity, opacity, opacity, opacity};

      RECT sourceRect = info.mSourceRect;

      sprites.Draw(srv, info.mDestRect, &sourceRect, tint);
    }
  }
  TraceLoggingWriteStop(
    activity,
    "OpenXRD3D11Kneeboard::RenderLayers()",
    TraceLoggingValue(true, "Success"));
  return true;
}

SHM::CachedReader* OpenXRD3D11Kneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
