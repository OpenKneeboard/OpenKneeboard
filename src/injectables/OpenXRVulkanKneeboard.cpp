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

  mAllocator = vulkanAllocator;

  mVK = std::make_unique<Vulkan::Dispatch>(
    binding.instance, pfnVkGetInstanceProcAddr);
  mVKInstance = binding.instance;
  mPhysicalDevice = binding.physicalDevice;
  mDevice = binding.device;
  mQueueFamilyIndex = binding.queueFamilyIndex;
  mQueueIndex = binding.queueIndex;
  mVK->GetDeviceQueue(mDevice, mQueueFamilyIndex, mQueueIndex, &mQueue);

  mSpriteBatch = std::make_unique<Vulkan::SpriteBatch>(
    mVK.get(),
    mPhysicalDevice,
    mDevice,
    mAllocator,
    mQueueFamilyIndex,
    mQueueIndex);

  VkCommandPoolCreateInfo poolCreateInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
      | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = mQueueFamilyIndex,
  };
  mCommandPool
    = mVK->make_unique<VkCommandPool>(mDevice, &poolCreateInfo, nullptr);
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRVulkanKneeboard()");
  this->WaitForAllFences();
}

XrSwapchain OpenXRVulkanKneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size) {
  if (mSwapchainResources) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Asked to create a second swapchain");
  }

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  const auto vkImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
  const auto vkImageViewFormat = VK_FORMAT_B8G8R8A8_UNORM;
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT
      | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
    .format = vkImageFormat,
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

  if (imageCount > mCommandBuffers.size()) {
    const auto oldLength = mCommandBuffers.size();
    VkCommandBufferAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = mCommandPool.get(),
      .commandBufferCount = static_cast<uint32_t>(imageCount - oldLength),
    };
    mCommandBuffers.resize(imageCount);
    check_vkresult(mVK->AllocateCommandBuffers(
      mDevice, &allocInfo, &mCommandBuffers.at(oldLength)));
  }

  VkImageViewCreateInfo viewCreateInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = vkImageViewFormat,
    .subresourceRange = VkImageSubresourceRange {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1,
    },
  };
  VkFenceCreateInfo fenceCreateInfo {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  std::vector<SwapchainBufferResources> buffers;
  for (auto& xrImage: images) {
    viewCreateInfo.image = xrImage.image;
    buffers.push_back(SwapchainBufferResources {
      .mImage = xrImage.image,
      .mImageView
      = mVK->make_unique<VkImageView>(mDevice, &viewCreateInfo, mAllocator),
      .mCompletionFence
      = mVK->make_unique<VkFence>(mDevice, &fenceCreateInfo, mAllocator),
      .mCommandBuffer = mCommandBuffers.at(buffers.size()),
    });
  }
  mSwapchainResources = {swapchain, std::move(buffers), size};

  mSHM.InitializeCache(
    mVK.get(),
    mVKInstance,
    mDevice,
    mPhysicalDevice,
    mQueueFamilyIndex,
    mQueueIndex,
    mAllocator,
    imageCount);

  return swapchain;
}

void OpenXRVulkanKneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!(mSwapchainResources && mSwapchainResources->mSwapchain == swapchain))
    [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Asked to destroy an inactive swapchain");
  }

  this->WaitForAllFences();

  mSwapchainResources = {};
}

void OpenXRVulkanKneeboard::WaitForAllFences() {
  if (!mSwapchainResources) {
    return;
  }
  const auto& buffers = mSwapchainResources->mBufferResources;

  if (buffers.empty()) {
    return;
  }

  std::vector<VkFence> fences;
  for (const auto& buffer: buffers) {
    fences.push_back(buffer.mCompletionFence.get());
  }

  check_vkresult(mVK->WaitForFences(
    mDevice, std::size(fences), fences.data(), true, ~(0ui64)));
}

void OpenXRVulkanKneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainIndex,
  const SHM::Snapshot& snapshot,
  const PixelRect* const destRects,
  const float* const opacities) {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRD3D12Kneeboard::RenderLayers()");

  if (!(mSwapchainResources && mSwapchainResources->mSwapchain == swapchain))
    [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Asked to render to wrong swapchain");
  }

  const auto& sr = *mSwapchainResources;
  const auto& br = sr.mBufferResources.at(swapchainIndex);

  auto source = snapshot.GetTexture<SHM::Vulkan::Texture>();
  auto sourceImage = source->GetVKImage();
  auto dest = br.mImageView.get();

  auto cb = br.mCommandBuffer;

  const VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  check_vkresult(mVK->BeginCommandBuffer(cb, &beginInfo));
  mSpriteBatch->Begin(cb, dest, sr.mDimensions);
  mSpriteBatch->Clear();

  const auto layerCount = snapshot.GetLayerCount();
  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    const auto sourceRect
      = snapshot.GetLayerConfig(layerIndex)->mLocationOnTexture;
    const auto& destRect = destRects[layerIndex];
    const Vulkan::Opacity opacity {opacities[layerIndex]};
    mSpriteBatch->Draw(
      source->GetVKImageView(),
      source->GetDimensions(),
      sourceRect,
      destRect,
      opacity);
  }

  mSpriteBatch->End();
  check_vkresult(mVK->EndCommandBuffer(cb));

  const auto fence = br.mCompletionFence.get();
  check_vkresult(mVK->ResetFences(mDevice, 1, &fence));

  const VkSemaphore waitSemaphores[] {source->GetReadySemaphore()};
  const uint64_t waitSemaphoreValues[] {source->GetReadySemaphoreValue()};

  const VkTimelineSemaphoreSubmitInfoKHR semaphoreInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
    .waitSemaphoreValueCount = std::size(waitSemaphoreValues),
    .pWaitSemaphoreValues = waitSemaphoreValues,
  };

  const VkPipelineStageFlags semaphoreStages {
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};

  const VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &semaphoreInfo,
    .waitSemaphoreCount = std::size(waitSemaphores),
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = &semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &cb,
  };
  check_vkresult(mVK->QueueSubmit(mQueue, 1, &submitInfo, fence));
}

SHM::CachedReader* OpenXRVulkanKneeboard::GetSHM() {
  return &mSHM;
}

}// namespace OpenKneeboard
