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
#define VK_USE_PLATFORM_WIN32_KHR
#define XR_USE_GRAPHICS_API_VULKAN

#include "OpenXRKneeboard.h"

#include <OpenKneeboard/SHM/D3D11.h>
#include <OpenKneeboard/SHM/Vulkan.h>
#include <OpenKneeboard/Vulkan.h>

#include <OpenKneeboard/config.h>

#include <d3d11_4.h>

// clang-format off
#include <vulkan/vulkan.h>
#include <openxr/openxr_platform.h>
// clang-format on

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
  virtual SHM::CachedReader* GetSHM() override;
  virtual XrSwapchain CreateSwapchain(
    XrSession,
    const PixelSize&,
    const VRRenderConfig::Quirks&) override;
  virtual void ReleaseSwapchainResources(XrSwapchain) override;
  virtual void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    uint8_t layerCount,
    SHM::LayerSprite* layers) override;

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() override;

 private:
  SHM::Vulkan::CachedReader mSHM;

  std::unique_ptr<OpenKneeboard::Vulkan::Dispatch> mVK;

  using DeviceResources = SHM::Vulkan::Renderer::DeviceResources;
  using SwapchainResources = SHM::Vulkan::Renderer::SwapchainResources;
  std::unique_ptr<DeviceResources> mDeviceResources;
  std::unordered_map<XrSwapchain, std::unique_ptr<SwapchainResources>>
    mSwapchainResources;
};

}// namespace OpenKneeboard
