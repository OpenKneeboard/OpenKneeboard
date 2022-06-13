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
#include <shims/winrt.h>

#include "OpenXRNext.h"

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrSession session,
  const std::shared_ptr<OpenXRNext>& next,
  ID3D11Device* device)
  : OpenXRKneeboard(session, next), mDevice(device) {
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

  auto oxr = next.get();

  auto nextResult
    = oxr->xrCreateSwapchain(session, &swapchainInfo, &mSwapchain);
  if (nextResult != XR_SUCCESS) {
    mSwapchain = nullptr;
    dprintf("Failed to create swapchain: {}", nextResult);
    return;
  }

  uint32_t imageCount = 0;
  nextResult
    = oxr->xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || nextResult != XR_SUCCESS) {
    dprintf("No images in swapchain: {}", nextResult);
    return;
  }

  dprintf("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D11KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D11KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
    });
  nextResult = oxr->xrEnumerateSwapchainImages(
    mSwapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to enumerate images in swapchain: {}", nextResult);
    oxr->xrDestroySwapchain(mSwapchain);
    mSwapchain = nullptr;
    return;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
    dprint("Swap chain is not a D3D11 swapchain");
    OPENKNEEBOARD_BREAK;
    oxr->xrDestroySwapchain(mSwapchain);
    mSwapchain = nullptr;
    return;
  }

  mRenderTargetViews.resize(imageCount);
  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };
  for (size_t i = 0; i < imageCount; ++i) {
#ifdef DEBUG
    if (images.at(i).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
      OPENKNEEBOARD_BREAK;
    }
#endif
    winrt::check_hresult(mDevice->CreateRenderTargetView(
      images.at(i).texture, &rtvd, mRenderTargetViews.at(i).put()));
  }

  dprint("Initialized kneeboard");
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
  dprintf("{}", __FUNCTION__);
  if (mSwapchain) {
    mOpenXR->xrDestroySwapchain(mSwapchain);
  }
}

XrResult OpenXRD3D11Kneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) {
  if (frameEndInfo->layerCount == 0) {
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto snapshot = mSHM.MaybeGet();
  if (!mSwapchain) {
    throw std::logic_error("Missing swapchain");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }
  if (!snapshot.IsValid()) {
    // Don't spam: expected, if OpenKneeboard isn't running
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto config = snapshot.GetConfig();
  const uint8_t layerIndex = 0;
  const auto& layer = *snapshot.GetLayerConfig(0);

  const auto displayTime = frameEndInfo->displayTime;
  const auto renderParams
    = this->GetRenderParameters(snapshot, layer, this->GetHMDPose(displayTime));
  this->Render(snapshot, renderParams);

  std::vector<const XrCompositionLayerBaseHeader*> nextLayers;
  std::copy(
    frameEndInfo->layers,
    &frameEndInfo->layers[frameEndInfo->layerCount],
    std::back_inserter(nextLayers));

  static_assert(
    SHM::SHARED_TEXTURE_IS_PREMULTIPLIED,
    "Use premultiplied alpha in shared texture, or pass "
    "XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT");
  XrCompositionLayerQuad xrLayer {
    .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
    .next = nullptr,
    .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT 
      | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
    .space = mLocalSpace,
    .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
    .subImage = {
      .swapchain = mSwapchain,
      .imageRect = {
        {0, 0},
        {static_cast<int32_t>(layer.mImageWidth), static_cast<int32_t>(layer.mImageHeight)},
      },
      .imageArrayIndex = 0,
    },
    .pose = this->GetXrPosef(renderParams.mKneeboardPose),
    .size = { renderParams.mKneeboardSize.x, renderParams.mKneeboardSize.y },
  };
  nextLayers.push_back(
    reinterpret_cast<XrCompositionLayerBaseHeader*>(&xrLayer));

  XrFrameEndInfo nextFrameEndInfo {*frameEndInfo};
  nextFrameEndInfo.layers = nextLayers.data();
  nextFrameEndInfo.layerCount = static_cast<uint32_t>(nextLayers.size());

  auto nextResult = mOpenXR->xrEndFrame(session, &nextFrameEndInfo);
  if (nextResult != XR_SUCCESS) {
    OPENKNEEBOARD_BREAK;
  }
  return nextResult;
}

void OpenXRD3D11Kneeboard::Render(
  const SHM::Snapshot& snapshot,
  const VRKneeboard::RenderParameters& params) {
  static uint64_t sCacheKey = ~(0ui64);
  if (params.mCacheKey == sCacheKey) {
    return;
  }

  auto oxr = mOpenXR.get();

  auto config = snapshot.GetConfig();
  uint32_t textureIndex;
  auto nextResult
    = oxr->xrAcquireSwapchainImage(mSwapchain, nullptr, &textureIndex);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to acquire swapchain image: {}", nextResult);
    return;
  }

  XrSwapchainImageWaitInfo waitInfo {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
    .timeout = XR_INFINITE_DURATION,
  };
  nextResult = oxr->xrWaitSwapchainImage(mSwapchain, &waitInfo);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to wait for swapchain image: {}", nextResult);
    nextResult = oxr->xrReleaseSwapchainImage(mSwapchain, nullptr);
    if (nextResult != XR_SUCCESS) {
      dprintf("Failed to release swapchain image: {}", nextResult);
    }
    return;
  }

  {
    auto sharedTexture = snapshot.GetLayerTexture(mDevice, textureIndex);
    if (!sharedTexture.IsValid()) {
      dprint("Failed to get shared texture");
      return;
    }
    D3D11::CopyTextureWithOpacity(
      mDevice,
      sharedTexture.GetShaderResourceView(),
      mRenderTargetViews.at(textureIndex).get(),
      params.mKneeboardOpacity);
  }
  nextResult = oxr->xrReleaseSwapchainImage(mSwapchain, nullptr);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to release swapchain: {}", nextResult);
  }
  sCacheKey = params.mCacheKey;
}

}// namespace OpenKneeboard
