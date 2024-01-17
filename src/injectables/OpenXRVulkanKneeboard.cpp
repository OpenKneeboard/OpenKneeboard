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
  mDeviceResources = std::make_unique<DeviceResources>(
    mVK.get(),
    binding.device,
    binding.physicalDevice,
    vulkanAllocator,
    binding.queueFamilyIndex,
    binding.queueIndex);
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRVulkanKneeboard()");
}

winrt::com_ptr<ID3D11Device> OpenXRVulkanKneeboard::GetD3D11Device() {
  return mDeviceResources->mD3D11Device;
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

  std::vector<VkImage> vkImages;
  for (auto& xrImage: images) {
    vkImages.push_back(xrImage.image);
  }
  mSwapchainResources[swapchain] = std::make_unique<SwapchainResources>(
    mVK.get(), mDeviceResources.get(), size, imageCount, vkImages.data());

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
  uint8_t layerCount,
  SHM::LayerSprite* layers) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "OpenXRD3VulkanKneeboard::RenderLayers()");

  auto dr = mDeviceResources.get();
  auto sr = mSwapchainResources.at(swapchain).get();

  auto rdr = dr->mRendererResources.get();
  auto rsr = sr->mRendererResources.get();

  namespace R = SHM::D3D11::Renderer;
  R::BeginFrame(rdr, rsr, swapchainTextureIndex);
  R::ClearRenderTargetView(rdr, rsr, swapchainTextureIndex);
  R::Render(
    rdr, rsr, swapchainTextureIndex, mSHM, snapshot, layerCount, layers);
  R::EndFrame(rdr, rsr, swapchainTextureIndex);

  // Signal this once D3D11 work is done, then we pass it as a wait semaphore
  // to vkQueueSubmit
  const auto semaphoreValue = ++sr->mInteropFenceValue;
  dr->mD3D11ImmediateContext->Signal(
    sr->mD3D11InteropFence.get(), semaphoreValue);
  auto br = &sr->mBufferResources.at(swapchainTextureIndex);

  const auto interopImage = br->mVKInteropImage.get();
  const auto swapchainImage = br->mVKSwapchainImage;
  const auto vkCommandBuffer = br->mVKCommandBuffer;

  VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  check_vkresult(mVK->ResetCommandBuffer(vkCommandBuffer, 0));
  check_vkresult(mVK->BeginCommandBuffer(vkCommandBuffer, &beginInfo));

  VkImageMemoryBarrier inBarriers[] = {
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = interopImage,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      },
    },
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = swapchainImage,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      },
    },
  };

  mVK->CmdPipelineBarrier(
    vkCommandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ std::size(inBarriers),
    inBarriers);

  std::vector<VkImageCopy> regions;
  regions.reserve(layerCount);
  for (off_t i = 0; i < layerCount; ++i) {
    auto& layer = layers[i];

    // The interop layer is the 'destination', and the swapchain
    // image should be identical, so, we use mDestRect for both
    // source and dest
    const RECT r = layer.mDestRect;

    regions.push_back(
      VkImageCopy {
        .srcSubresource = VkImageSubresourceLayers {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
        .srcOffset = {r.left, r.top, 0},
        .dstSubresource = VkImageSubresourceLayers {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
        .dstOffset = {r.left, r.top, 0},
        .extent = {layer.mDestRect.mSize.mWidth, layer.mDestRect.mSize.mHeight, 1},
      }
    );
  }
  mVK->CmdCopyImage(
    vkCommandBuffer,
    interopImage,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    swapchainImage,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    regions.size(),
    regions.data());

  VkImageMemoryBarrier outBarriers[] = {
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .image = swapchainImage,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      },
    },
  };

  // Wait for copy to be complete, then...
  mVK->CmdPipelineBarrier(
    vkCommandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    /* dependency flags = */ {},
    /* memory barrier count = */ 0,
    /* memory barriers = */ nullptr,
    /* buffer barrier count = */ 0,
    /* buffer barriers = */ nullptr,
    /* image barrier count = */ std::size(outBarriers),
    outBarriers);
  check_vkresult(mVK->EndCommandBuffer(vkCommandBuffer));

  VkTimelineSemaphoreSubmitInfoKHR timelineSubmitInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .waitSemaphoreValueCount = 1,
    .pWaitSemaphoreValues = &semaphoreValue,
  };
  VkPipelineStageFlags semaphoreStages {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};
  VkSemaphore waitSemaphores[] = {sr->mVKInteropSemaphore.get()};
  VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &timelineSubmitInfo,
    .waitSemaphoreCount = std::size(waitSemaphores),
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = &semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &vkCommandBuffer,
  };

  VkFence fences[] = {sr->mVKCompletionFence.get()};
  check_vkresult(mVK->ResetFences(dr->mVKDevice, std::size(fences), fences));
  check_vkresult(mVK->QueueSubmit(
    dr->mVKQueue, 1, &submitInfo, sr->mVKCompletionFence.get()));

  TraceLoggingWriteStop(activity, "OpenXRD3VulkanKneeboard::RenderLayers()");
}// namespace OpenKneeboard

SHM::CachedReader* OpenXRVulkanKneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
