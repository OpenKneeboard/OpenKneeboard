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
  SHM::D3D11::CachedReader mSHM;
  XrGraphicsBindingVulkanKHR mBinding {};

  // We need this for the renderer..
  using RendererDeviceResources = SHM::D3D11::Renderer::DeviceResources;
  std::unique_ptr<RendererDeviceResources> mRendererDeviceResources;
  // ... but keep specialized versions that have interop capabilities
  winrt::com_ptr<ID3D11Device5> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext4> mD3D11ImmediateContext;

  const VkAllocationCallbacks* mVKAllocator {nullptr};
  VkDevice mVKDevice {nullptr};
  VkCommandPool mVKCommandPool {nullptr};
  VkQueue mVKQueue {nullptr};

  using RendererSwapchainResources = SHM::D3D11::Renderer::SwapchainResources;
  struct Interop {
    std::unique_ptr<RendererSwapchainResources> mRendererResources;

    std::vector<VkImage> mVKImages {};
    VkFence mVKCompletionFence {};

    // These point to the same GPU fence/semaphore
    VkSemaphore mVKInteropSemaphore {};
    winrt::com_ptr<ID3D11Fence> mD3D11InteropFence {};
    winrt::handle mD3D11InteropFenceHandle {};
    // Next value for the fence/semaphore
    uint64_t mInteropValue {};
  };
  struct SwapchainResources {
    Interop mInterop {};
    std::vector<VkImage> mImages;
    std::vector<VkCommandBuffer> mVKCommandBuffers;
    PixelSize mSize;
  };
  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;
  void InitInterop(
    uint32_t textureCount,
    const PixelSize& textureSize,
    Interop*) noexcept;

  std::unique_ptr<OpenKneeboard::Vulkan::Dispatch> mVK;

  void InitializeD3D11();
  void InitializeVulkan(PFN_vkGetInstanceProcAddr);
};

}// namespace OpenKneeboard
