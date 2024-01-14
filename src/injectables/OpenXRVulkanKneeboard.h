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

#include <OpenKneeboard/config.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include "OpenXRKneeboard.h"

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
  };

  static constexpr const char* VK_DEVICE_EXTENSIONS[] {
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
  };

 protected:
  virtual bool ConfigurationsAreCompatible(
    const VRRenderConfig& initial,
    const VRRenderConfig& current) const override;
  virtual XrSwapchain CreateSwapChain(
    XrSession,
    const VRRenderConfig&,
    uint8_t layerIndex) override;
  virtual bool Render(
    XrSwapchain swapchain,
    const SHM::Snapshot& snapshot,
    uint8_t layerIndex,
    const VRKneeboard::RenderParameters&) override;

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() override;

 private:
  winrt::com_ptr<ID3D11Device> mD3D11Device;

  struct Interop {
    winrt::com_ptr<ID3D11Texture2D> mD3D11Texture;
    winrt::com_ptr<ID3D11RenderTargetView> mD3D11RenderTargetView;

    VkImage mVKImage {};
    VkFence mVKFence {};
  };
  std::array<Interop, MaxLayers> mLayerInterop;

  void InitInterop(const XrGraphicsBindingVulkanKHR&, Interop*);

  const VkAllocationCallbacks* mVKAllocator {nullptr};
  VkDevice mVKDevice {nullptr};
  VkCommandPool mVKCommandPool {nullptr};
  VkCommandBuffer mVKCommandBuffer {nullptr};
  VkQueue mVKQueue {nullptr};
  std::array<std::vector<VkImage>, MaxLayers> mImages;

#define OPENKNEEBOARD_VK_FUNCS \
  IT(vkGetPhysicalDeviceProperties2) \
  IT(vkCreateCommandPool) \
  IT(vkAllocateCommandBuffers) \
  IT(vkBeginCommandBuffer) \
  IT(vkEndCommandBuffer) \
  IT(vkCmdCopyImage) \
  IT(vkCmdPipelineBarrier) \
  IT(vkCreateImage) \
  IT(vkGetImageMemoryRequirements2) \
  IT(vkGetPhysicalDeviceMemoryProperties) \
  IT(vkGetMemoryWin32HandlePropertiesKHR) \
  IT(vkBindImageMemory2) \
  IT(vkAllocateMemory) \
  IT(vkCreateFence) \
  IT(vkResetFences) \
  IT(vkWaitForFences) \
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
