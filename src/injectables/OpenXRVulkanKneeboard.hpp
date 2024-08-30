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
#pragma once
#define XR_USE_GRAPHICS_API_VULKAN

#include "OpenXRKneeboard.hpp"

#include <OpenKneeboard/SHM/Vulkan.hpp>
#include <OpenKneeboard/Vulkan.hpp>
#include <OpenKneeboard/Vulkan/ExtendedCreateInfo.hpp>
#include <OpenKneeboard/Vulkan/SpriteBatch.hpp>

#include <OpenKneeboard/config.hpp>

#include <shims/vulkan/vulkan.h>

#include <openxr/openxr_platform.h>

#include <d3d11_4.h>

namespace OpenKneeboard {

class OpenXRVulkanKneeboard final : public OpenXRKneeboard {
 public:
  using VkInstanceCreateInfo = Vulkan::CombinedCreateInfo<
    Vulkan::SpriteBatch::InstanceCreateInfo,
    SHM::Vulkan::InstanceCreateInfo>;
  using VkDeviceCreateInfo = Vulkan::CombinedCreateInfo<
    Vulkan::SpriteBatch::DeviceCreateInfo,
    SHM::Vulkan::DeviceCreateInfo>;

  OpenXRVulkanKneeboard() = delete;
  OpenXRVulkanKneeboard(
    XrInstance,
    XrSystemId,
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingVulkanKHR&,
    const VkAllocationCallbacks*,
    PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr);
  ~OpenXRVulkanKneeboard();

 protected:
  virtual SHM::CachedReader* GetSHM() override;
  virtual XrSwapchain CreateSwapchain(XrSession, const PixelSize&) override;
  virtual void ReleaseSwapchainResources(XrSwapchain) override;
  virtual void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const std::span<SHM::LayerSprite>& layers) override;

 private:
  std::unique_ptr<OpenKneeboard::Vulkan::Dispatch> mVK;

  const VkAllocationCallbacks* mAllocator {nullptr};
  VkInstance mVKInstance {VK_NULL_HANDLE};
  VkPhysicalDevice mPhysicalDevice {VK_NULL_HANDLE};
  VkDevice mDevice {VK_NULL_HANDLE};
  VkQueue mQueue {VK_NULL_HANDLE};
  uint32_t mQueueFamilyIndex {~(0ui32)};
  uint32_t mQueueIndex {~(0ui32)};

  std::unique_ptr<Vulkan::SpriteBatch> mSpriteBatch;

  template <class T>
  using unique_vk = Vulkan::unique_vk<T>;

  unique_vk<VkCommandPool> mCommandPool;
  std::vector<VkCommandBuffer> mCommandBuffers;

  struct SwapchainBufferResources {
    VkImage mImage {VK_NULL_HANDLE};
    unique_vk<VkImageView> mImageView;
    unique_vk<VkFence> mCompletionFence;
    VkCommandBuffer mCommandBuffer {VK_NULL_HANDLE};
  };

  struct SwapchainResources {
    XrSwapchain mSwapchain {XR_NULL_HANDLE};
    std::vector<SwapchainBufferResources> mBufferResources;
    PixelSize mDimensions;
  };

  std::optional<SwapchainResources> mSwapchainResources;

  SHM::Vulkan::CachedReader mSHM {SHM::ConsumerKind::OpenXR};

  void WaitForAllFences();
};

}// namespace OpenKneeboard
