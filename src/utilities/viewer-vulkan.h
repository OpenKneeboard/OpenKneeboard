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

#include "viewer.h"

#include <OpenKneeboard/SHM/Vulkan.h>
#include <OpenKneeboard/Vulkan/SpriteBatch.h>

#include <OpenKneeboard/handles.h>

#include <cinttypes>

namespace OpenKneeboard::Viewer {
class VulkanRenderer final : public Renderer {
 public:
  VulkanRenderer() = delete;
  VulkanRenderer(uint64_t luid);
  virtual ~VulkanRenderer();

  virtual SHM::CachedReader* GetSHM() override;
  virtual std::wstring_view GetName() const noexcept override;

  virtual void Initialize(uint8_t swapchainLength) override;

  virtual void SaveTextureToFile(
    SHM::IPCClientTexture*,
    const std::filesystem::path&) override;

  virtual uint64_t Render(
    SHM::IPCClientTexture* sourceTexture,
    const PixelRect& sourceRect,
    HANDLE destTexture,
    const PixelSize& destTextureDimensions,
    const PixelRect& destRect,
    HANDLE fence,
    uint64_t fenceValueIn) override;

 private:
  unique_hmodule mVulkanLoader;

  template <class T>
  using unique_vk = OpenKneeboard::Vulkan::unique_vk<T>;

  unique_vk<VkInstance> mVKInstance;
  std::unique_ptr<OpenKneeboard::Vulkan::Dispatch> mVK;
#ifdef DEBUG
  unique_vk<VkDebugUtilsMessengerEXT> mVKDebugMessenger;
#endif

  VkPhysicalDevice mVKPhysicalDevice {VK_NULL_HANDLE};

  uint32_t mQueueFamilyIndex;

  unique_vk<VkDevice> mDevice;
  VkQueue mQueue;

  unique_vk<VkCommandPool> mCommandPool;
  VkCommandBuffer mCommandBuffer {};
  unique_vk<VkFence> mCompletionFence;

  HANDLE mDestHandle {};
  unique_vk<VkImage> mDestImage;
  unique_vk<VkDeviceMemory> mDestImageMemory;
  unique_vk<VkImageView> mDestImageView;

  HANDLE mSemaphoreHandle {};
  unique_vk<VkSemaphore> mSemaphore;

  // Last as it caches some Vulkan resources; as Vulkan doesn't internally use
  // refcounting, we need to make sure these are released before the `unique_vk`
  // above.
  SHM::Vulkan::CachedReader mSHM {SHM::ConsumerKind::Viewer};
  std::unique_ptr<OpenKneeboard::Vulkan::SpriteBatch> mSpriteBatch;

  void InitializeSemaphore(HANDLE);
  void InitializeDest(HANDLE, const PixelSize&);

  void SaveTextureToFile(
    const PixelSize& dimensions,
    VkImage source,
    VkImageLayout layout,
    VkSemaphore waitSemaphore,
    uint64_t waitSemaphoreValue,
    const std::filesystem::path&);
};

}// namespace OpenKneeboard::Viewer