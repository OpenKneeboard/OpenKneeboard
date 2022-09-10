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

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <d3d11.h>
#include <shims/winrt/base.h>

#include "OpenXRNext.h"

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D11KHR& binding)
  : OpenXRKneeboard(session, runtimeID, next), mDevice(binding.device) {
  dprintf("{}", __FUNCTION__);
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
}

bool OpenXRD3D11Kneeboard::FlagsAreCompatible(
  VRRenderConfig::Flags,
  VRRenderConfig::Flags) const {
  return true;
}

XrSwapchain OpenXRD3D11Kneeboard::CreateSwapChain(
  XrSession session,
  VRRenderConfig::Flags,
  uint8_t layerIndex) {
  dprintf("{}", __FUNCTION__);

  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
    .format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    .sampleCount = 1,
    .width = TextureWidth,
    .height = TextureHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  auto oxr = this->GetOpenXR();

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
  if (nextResult != XR_SUCCESS) {
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

  mRenderTargetViews.at(layerIndex).resize(imageCount);
  for (size_t i = 0; i < imageCount; ++i) {
#ifdef DEBUG
    if (images.at(i).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
      OPENKNEEBOARD_BREAK;
    }
#endif
    mRenderTargetViews.at(layerIndex).at(i)
      = std::make_shared<D3D11::RenderTargetViewFactory>(
        mDevice, images.at(i).texture);
  }

  return swapchain;
}

bool OpenXRD3D11Kneeboard::Render(
  XrSwapchain swapchain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters& renderParameters) {
  return OpenXRD3D11Kneeboard::Render(
    this->GetOpenXR(),
    mDevice,
    mRenderTargetViews.at(layerIndex),
    swapchain,
    snapshot,
    layerIndex,
    renderParameters);
}

bool OpenXRD3D11Kneeboard::Render(
  OpenXRNext* oxr,
  ID3D11Device* device,
  const std::vector<std::shared_ptr<D3D11::IRenderTargetViewFactory>>&
    renderTargetViews,
  XrSwapchain swapchain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters& renderParameters) {
  auto config = snapshot.GetConfig();
  uint32_t textureIndex;
  auto nextResult
    = oxr->xrAcquireSwapchainImage(swapchain, nullptr, &textureIndex);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to acquire swapchain image: {}", nextResult);
    return false;
  }

  XrSwapchainImageWaitInfo waitInfo {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
    .timeout = XR_INFINITE_DURATION,
  };
  nextResult = oxr->xrWaitSwapchainImage(swapchain, &waitInfo);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to wait for swapchain image: {}", nextResult);
    nextResult = oxr->xrReleaseSwapchainImage(swapchain, nullptr);
    if (nextResult != XR_SUCCESS) {
      dprintf("Failed to release swapchain image: {}", nextResult);
    }
    return false;
  }

  {
    auto sharedTexture = snapshot.GetLayerTexture(device, layerIndex);
    if (!sharedTexture.IsValid()) {
      dprint("Failed to get shared texture");
      return false;
    }
    auto rtv = renderTargetViews.at(textureIndex)->Get();
    D3D11::CopyTextureWithOpacity(
      device,
      sharedTexture.GetShaderResourceView(),
      rtv->Get(),
      renderParameters.mKneeboardOpacity);
  }
  nextResult = oxr->xrReleaseSwapchainImage(swapchain, nullptr);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to release swapchain: {}", nextResult);
  }
  return true;
}

}// namespace OpenKneeboard
