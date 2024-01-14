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

#include "OpenXRKneeboard.h"

#include <OpenKneeboard/config.h>

#include <d3d11_4.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

struct XrGraphicsBindingVulkanKHR;

namespace OpenKneeboard {

class OpenXRVulkanKneeboard final : public OpenXRKneeboard {
 public:
  OpenXRVulkanKneeboard() = delete;
  OpenXRVulkanKneeboard(
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingVulkanKHR&,
    const VkAllocationCallbacks*,
    PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr);
  ~OpenXRVulkanKneeboard();

  static constexpr const char* VK_INSTANCE_EXTENSIONS[] {
    VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
  };

  static constexpr const char* VK_DEVICE_EXTENSIONS[] {
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
  };

 protected:
  virtual bool ConfigurationsAreCompatible(
    const VRRenderConfig& initial,
    const VRRenderConfig& current) const override;
  virtual XrSwapchain CreateSwapchain(
    XrSession,
    const PixelSize&,
    const VRRenderConfig::Quirks&) override;
  virtual void ReleaseSwapchainResources(XrSwapchain) override;
  virtual bool RenderLayer(
    XrSwapchain swapchain,
    const SHM::Snapshot& snapshot,
    uint8_t layerIndex,
    const VRKneeboard::RenderParameters&) override;

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() override;

 private:
  winrt::com_ptr<ID3D11Device5> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext4> mD3D11ImmediateContext;

  struct Interop {
    winrt::com_ptr<ID3D11Texture2D> mD3D11Texture;
    winrt::com_ptr<ID3D11RenderTargetView> mD3D11RenderTargetView;

    VkImage mVKImage {};
    VkFence mVKCompletionFence {};

    // These point to the same GPU fence/semaphore
    VkSemaphore mVKInteropSemaphore {};
    winrt::com_ptr<ID3D11Fence> mD3D11InteropFence {};
    winrt::handle mD3D11InteropFenceHandle {};
    // Next value for the fence/semaphore
    uint64_t mInteropValue {};
  };
  std::array<Interop, MaxLayers> mLayerInterop;

  void InitInterop(const XrGraphicsBindingVulkanKHR&, Interop*) noexcept;

  const VkAllocationCallbacks* mVKAllocator {nullptr};
  VkDevice mVKDevice {nullptr};
  VkCommandPool mVKCommandPool {nullptr};
  VkCommandBuffer mVKCommandBuffer {nullptr};
  VkQueue mVKQueue {nullptr};
  std::unordered_map<XrSwapchain, std::vector<VkImage>> mImages;

#define OPENKNEEBOARD_VK_FUNCS \
  IT(vkGetPhysicalDeviceProperties2) \
  IT(vkCreateCommandPool) \
  IT(vkAllocateCommandBuffers) \
  IT(vkBeginCommandBuffer) \
  IT(vkEndCommandBuffer) \
  IT(vkCmdCopyImage) \
  IT(vkCmdPipelineBarrier) \
  IT(vkCreateImage) \
  IT(vkDestroyImage) \
  IT(vkGetImageMemoryRequirements2) \
  IT(vkGetPhysicalDeviceMemoryProperties) \
  IT(vkGetMemoryWin32HandlePropertiesKHR) \
  IT(vkBindImageMemory2) \
  IT(vkAllocateMemory) \
  IT(vkCreateFence) \
  IT(vkResetFences) \
  IT(vkWaitForFences) \
  IT(vkCreateSemaphore) \
  IT(vkImportSemaphoreWin32HandleKHR) \
  IT(vkGetSemaphoreWin32HandleKHR) \
  IT(vkQueueSubmit) \
  IT(vkGetDeviceQueue)
#define IT(func) PFN_##func mPFN_##func {nullptr};
  OPENKNEEBOARD_VK_FUNCS
#undef IT

  void InitializeD3D11(const XrGraphicsBindingVulkanKHR&);
  void InitializeVulkan(
    const XrGraphicsBindingVulkanKHR&,
    PFN_vkGetInstanceProcAddr);
};

}// namespace OpenKneeboard
