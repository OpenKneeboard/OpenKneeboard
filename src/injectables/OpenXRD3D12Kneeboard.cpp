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

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <shims/winrt.h>

#include "OpenXRNext.h"

#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

OpenXRD3D12Kneeboard::OpenXRD3D12Kneeboard(
  XrSession session,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D12KHR& binding)
  : OpenXRKneeboard(session, next), mDevice(binding.device) {
  dprintf("{}", __FUNCTION__);

#ifdef DEBUG
  winrt::com_ptr<ID3D12Debug> debug;
  D3D12GetDebugInterface(IID_PPV_ARGS(debug.put()));
  if (debug) {
    dprint("Enabling D3D12 debug layer");
    debug->EnableDebugLayer();
  }
#endif

  UINT flags = 0;
#ifdef DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  D3D11On12CreateDevice(
    mDevice,
    flags,
    nullptr,
    0,
    nullptr,
    0,
    1,
    m11on12.put(),
    m11on12Context.put(),
    nullptr);
}

OpenXRD3D12Kneeboard::~OpenXRD3D12Kneeboard() = default;

XrSwapchain OpenXRD3D12Kneeboard::CreateSwapChain(
  XrSession session,
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

  mRenderTargetViews.at(layerIndex).resize(imageCount);
  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };

  for (size_t i = 0; i < imageCount; ++i) {
#ifdef DEBUG
    if (images.at(i).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
      OPENKNEEBOARD_BREAK;
    }
#endif
    D3D11_RESOURCE_FLAGS resourceFlags11 {D3D11_BIND_RENDER_TARGET};
    winrt::com_ptr<ID3D11Texture2D> texture11;
    winrt::check_hresult(m11on12.as<ID3D11On12Device2>()->CreateWrappedResource(
      images.at(i).texture,
      &resourceFlags11,
      D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COMMON,
      IID_PPV_ARGS(texture11.put())));

    winrt::check_hresult(m11on12->CreateRenderTargetView(
      texture11.get(), &rtvd, mRenderTargetViews.at(layerIndex).at(i).put()));
  }
  dprintf(
    "Created {} 11on12 RenderTargetViews for layer {}", imageCount, layerIndex);

  return swapchain;
}

bool OpenXRD3D12Kneeboard::Render(
  XrSwapchain swapchain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters& renderParameters) {
  auto oxr = this->GetOpenXR();

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
    auto device = m11on12.get();
    auto sharedTexture = snapshot.GetLayerTexture(device, layerIndex);
    if (!sharedTexture.IsValid()) {
      dprint("Failed to get shared texture");
      return false;
    }
    D3D11::CopyTextureWithOpacity(
      device,
      sharedTexture.GetShaderResourceView(),
      mRenderTargetViews.at(layerIndex).at(textureIndex).get(),
      renderParameters.mKneeboardOpacity);
  }
  nextResult = oxr->xrReleaseSwapchainImage(swapchain, nullptr);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to release swapchain: {}", nextResult);
  }
  return true;
}

}// namespace OpenKneeboard
