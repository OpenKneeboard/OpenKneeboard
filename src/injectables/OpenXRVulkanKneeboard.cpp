/*
 * OpenKneeboard
 *
 * Copyright (C) 2022-present Fred Emmott <fred@fredemmott.com>
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

#include "OpenXRVulkanKneeboard.h"

#include "OpenXRNext.h"

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <vulkan/vulkan.h>

#include <d3d11_4.h>
#include <dxgi1_6.h>

template <class CharT>
struct std::formatter<VkResult, CharT> : std::formatter<int, CharT> {};

using OpenKneeboard::Vulkan::check_vkresult;
using OpenKneeboard::Vulkan::VK_FAILED;

namespace OpenKneeboard {

OpenXRVulkanKneeboard::OpenXRVulkanKneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingVulkanKHR& binding,
  const VkAllocationCallbacks* vulkanAllocator,
  PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr)
  : OpenXRKneeboard(session, runtimeID, next) {
  dprintf("{}", __FUNCTION__);
  TraceLoggingWrite(gTraceProvider, "OpenXRVulkanKneeboard()");

  mVK = std::make_unique<Vulkan::Dispatch>(
    binding.instance, pfnVkGetInstanceProcAddr);
  mBinding = binding;
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRVulkanKneeboard()");
}

XrSwapchain OpenXRVulkanKneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size,
  const VRRenderConfig::Quirks&) {
  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  const auto vkFormat = VK_FORMAT_B8G8R8A8_SRGB;
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
    .format = vkFormat,
    .sampleCount = 1,
    .width = size.mWidth,
    .height = size.mHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  auto oxr = this->GetOpenXR();

  XrSwapchain swapchain {nullptr};
  {
    const auto ret
      = oxr->xrCreateSwapchain(session, &swapchainInfo, &swapchain);
    if (XR_FAILED(ret)) {
      dprintf("next->xrCreateSwapchain failed: {}", ret);
      return nullptr;
    }
  }

  uint32_t imageCount = 0;
  {
    const auto res
      = oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
    if (XR_FAILED(res) || imageCount == 0) {
      dprintf("No images in swapchain: {}", res);
      return nullptr;
    }
  }

  dprintf("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageVulkanKHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageVulkanKHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR,
    });
  {
    const auto res = oxr->xrEnumerateSwapchainImages(
      swapchain,
      imageCount,
      &imageCount,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    if (XR_FAILED(res)) {
      dprintf("Failed to enumerate images in swapchain: {}", res);
      oxr->xrDestroySwapchain(swapchain);
      return nullptr;
    }
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) {
    dprint("Swap chain is not a Vulkan swapchain");
    OPENKNEEBOARD_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  std::vector<SwapchainBufferResources> buffers;
  for (auto& xrImage: images) {
    buffers.push_back({xrImage.image});
  }
  mSwapchainResources[swapchain] = {std::move(buffers), size};

  return swapchain;
}

void OpenXRVulkanKneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!mSwapchainResources.contains(swapchain)) {
    return;
  }

  mSwapchainResources.erase(swapchain);
}

void OpenXRVulkanKneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  const SHM::Snapshot& snapshot,
  const PixelRect* const destRects,
  const float* const opacities) {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D12Kneeboard::RenderLayers()");

  auto source = snapshot.GetTexture<SHM::Vulkan::Texture>();
  // TODO
}

SHM::CachedReader* OpenXRVulkanKneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
