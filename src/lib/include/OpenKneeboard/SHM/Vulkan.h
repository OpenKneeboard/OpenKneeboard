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
#include <OpenKneeboard/Pixels.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SHM/D3D11.h>
#include <OpenKneeboard/Vulkan.h>

#include <shims/winrt/base.h>

#include <memory>

#include <d3d11_4.h>

namespace OpenKneeboard::SHM::Vulkan {

using SHM::D3D11::CachedReader;

// Usage:
// - create DeviceResources and SwapchainResources
// - Call BeginFrame(), Render() [, Render(), ...], EndFrame()
// - Optionally call ClearRenderTargetView() after BeginFrame(), depending on
// your needs.
namespace Renderer {

struct DeviceResources {
  DeviceResources() = delete;
  DeviceResources(
    OpenKneeboard::Vulkan::Dispatch*,
    VkDevice,
    VkPhysicalDevice,
    const VkAllocationCallbacks*,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex);

  OpenKneeboard::Vulkan::Dispatch* mVK {nullptr};

  VkDevice mVKDevice {};
  VkPhysicalDevice mVKPhysicalDevice {};
  const VkAllocationCallbacks* mVKAllocator {nullptr};

  uint32_t mVKQueueFamilyIndex {};
  uint32_t mVKQueueIndex {};

  VkQueue mVKQueue {};

  OpenKneeboard::Vulkan::unique_VkCommandPool mVKCommandPool {nullptr};

  // We render with D3D11, so we have some D3D11 and interop stuff too:

  using RendererDeviceResources = SHM::D3D11::Renderer::DeviceResources;
  std::unique_ptr<RendererDeviceResources> mRendererResources;
  // Keep specialized versions that have interop capabilities to avoid
  // QueryInterface() every frame
  winrt::com_ptr<ID3D11Device5> mD3D11Device;
  winrt::com_ptr<ID3D11DeviceContext4> mD3D11ImmediateContext;

 private:
  void InitializeD3D11();
};

struct SwapchainResources {
  SwapchainResources() = delete;
  SwapchainResources(
    DeviceResources*,
    const PixelSize&,
    uint32_t textureCount,
    VkImage* textures) noexcept;
  PixelSize mSize;

  struct BufferResources {
    VkImage mVKSwapchainImage {};
    VkCommandBuffer mVKCommandBuffer {};

    // Intermediate images that are accessible both to D3D11 and VK.
    // The ID3D11Texture2D's are stored in mRendererResources
    OpenKneeboard::Vulkan::unique_VkImage mVKInteropImage;

    // Regions that need to be copied from the interop image to the
    // swapchain image
    std::vector<PixelRect> mDirtyRects;
  };
  std::vector<BufferResources> mBufferResources;

  // Set when VK rendering is finished
  OpenKneeboard::Vulkan::unique_VkFence mVKCompletionFence {};

  // Use D3D11 for rendering as:
  // - there's nothing handy like DirectXTK::SpriteBatch for VK
  // - ... I need to learn more before rewriting this to directly use VK
  using RendererSwapchainResources = SHM::D3D11::Renderer::SwapchainResources;
  std::unique_ptr<RendererSwapchainResources> mRendererResources;

  // These point to the same GPU fence/semaphore; it marks the transition
  // from D3D11 to VK

  OpenKneeboard::Vulkan::unique_VkSemaphore mVKInteropSemaphore {};
  winrt::com_ptr<ID3D11Fence> mD3D11InteropFence {};
  winrt::handle mD3D11InteropFenceHandle {};
  uint64_t mInteropFenceValue {};

 private:
  void InitializeInterop(
    DeviceResources*,
    uint32_t textureCount,
    const PixelSize& textureSize) noexcept;
};

void BeginFrame(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

void ClearRenderTargetView(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

void Render(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex,
  const SHM::Vulkan::CachedReader&,
  const SHM::Snapshot&,
  size_t layerSpriteCount,
  LayerSprite* layerSprites);

void EndFrame(
  DeviceResources*,
  SwapchainResources*,
  uint8_t swapchainTextureIndex);

}// namespace Renderer

};// namespace OpenKneeboard::SHM::Vulkan