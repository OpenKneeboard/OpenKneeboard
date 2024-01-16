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

#include "OpenXRD3D12Kneeboard.h"

#include "OpenXRD3D11Kneeboard.h"
#include "OpenXRNext.h"

#include <OpenKneeboard/D3D11.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <string>

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>

#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

OpenXRD3D12Kneeboard::OpenXRD3D12Kneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D12KHR& binding)
  : OpenXRKneeboard(session, runtimeID, next), mSHM(binding.device) {
  dprintf("{}", __FUNCTION__);

  mDeviceResources
    = std::make_unique<DeviceResources>(binding.device, binding.queue);
}

OpenXRD3D12Kneeboard::~OpenXRD3D12Kneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRD3D12Kneeboard()");
}

bool OpenXRD3D12Kneeboard::ConfigurationsAreCompatible(
  const VRRenderConfig& initial,
  const VRRenderConfig& current) const {
  return true;
}

XrSwapchain OpenXRD3D12Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size,
  const VRRenderConfig::Quirks& quirks) {
  dprintf("{}", __FUNCTION__);

  auto oxr = this->GetOpenXR();

  auto formats = OpenXRD3D11Kneeboard::GetDXGIFormats(oxr, session);
  dprintf(
    "Creating swapchain with format {}",
    static_cast<INT>(formats.mTextureFormat));

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
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult
    = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || nextResult != XR_SUCCESS) {
    dprintf("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprintf("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D12KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D12KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR,
    });

  nextResult = oxr->xrEnumerateSwapchainImages(
    swapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to enumerate images in swapchain: {}", nextResult);
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
    dprint("Swap chain is not a D3D12 swapchain");
    OPENKNEEBOARD_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  std::vector<ID3D12Resource*> textures;
  textures.reserve(images.size());
  for (const auto& image: images) {
    textures.push_back(image.texture);
  }
  mSwapchainResources[swapchain] = std::make_unique<SwapchainResources>(
    mDeviceResources.get(),
    formats.mRenderTargetViewFormat,
    textures.size(),
    textures.data());

  return swapchain;
}

void OpenXRD3D12Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!mSwapchainResources.contains(swapchain)) {
    return;
  }
  mSwapchainResources.erase(swapchain);
}

bool OpenXRD3D12Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  uint8_t layerCount,
  LayerRenderInfo* layers) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "OpenXRD3D12Kneeboard::RenderLayers()");

  using Sprite = SHM::D3D12::Renderer::LayerSprite;
  std::vector<Sprite> sprites;
  sprites.reserve(layerCount);
  for (uint8_t i = 0; i < layerCount; ++i) {
    const auto& layer = layers[i];
    sprites.push_back(Sprite {
      .mLayerIndex = layer.mLayerIndex,
      .mDestRect = layer.mDestRect,
      .mOpacity = layer.mVR.mKneeboardOpacity,
    });
  }

  auto dr = mDeviceResources.get();
  auto sr = mSwapchainResources.at(swapchain).get();

  SHM::D3D12::Renderer::BeginFrame(dr, sr, swapchainTextureIndex);
  SHM::D3D12::Renderer::ClearRenderTargetView(dr, sr, swapchainTextureIndex);
  SHM::D3D12::Renderer::Render(
    dr,
    sr,
    swapchainTextureIndex,
    mSHM,
    snapshot,
    sprites.size(),
    sprites.data());
  SHM::D3D12::Renderer::EndFrame(dr, sr, swapchainTextureIndex);
  return true;
}

winrt::com_ptr<ID3D11Device> OpenXRD3D12Kneeboard::GetD3D11Device() {
  return mDeviceResources->mD3D11Device;
}

SHM::CachedReader* OpenXRD3D12Kneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
