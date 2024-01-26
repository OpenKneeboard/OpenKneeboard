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
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/Vulkan/Dispatch.h>
#include <OpenKneeboard/Vulkan/ExtendedCreateInfo.h>
#include <OpenKneeboard/Vulkan/smart-pointers.h>

namespace OpenKneeboard::SHM::Vulkan {

using OpenKneeboard::Vulkan::unique_vk;

class Texture final : public SHM::IPCClientTexture {
 public:
  Texture() = delete;
  Texture(
    OpenKneeboard::Vulkan::Dispatch*,
    VkPhysicalDevice,
    VkDevice,
    uint32_t queueFamilyIndex,
    const VkAllocationCallbacks*,
    VkFence completionFence,
    const PixelSize&,
    uint8_t swapchainIndex);
  virtual ~Texture();

  VkImage GetVKImage();
  VkImageView GetVKImageView();

  VkSemaphore GetReadySemaphore() const;
  uint64_t GetReadySemaphoreValue() const;

  void CopyFrom(
    VkQueue,
    VkCommandBuffer,
    VkImage image,
    VkSemaphore semaphore,
    uint64_t semaphoreValueIn,
    uint64_t sempahoreValueOut) noexcept;

 private:
  OpenKneeboard::Vulkan::Dispatch* mVK {nullptr};
  VkDevice mDevice {VK_NULL_HANDLE};
  VkPhysicalDevice mPhysicalDevice {VK_NULL_HANDLE};
  uint32_t mQueueFamilyIndex {~(0ui32)};
  const VkAllocationCallbacks* mAllocator {nullptr};

  VkFence mCompletionFence {VK_NULL_HANDLE};

  unique_vk<VkDeviceMemory> mImageMemory;
  unique_vk<VkImage> mImage;
  unique_vk<VkImageView> mImageView;

  // This is *NOT* the IPC semaphore - it is a dedicated semaphore
  // for clients to wait on.
  //
  // This is to decouple its' lifetime from the CachedReader's lifetime
  unique_vk<VkSemaphore> mReadySemaphore;
  uint64_t mReadySemaphoreValue {};

  void InitializeCacheImage();
  void InitializeReadySemaphore();
};

struct InstanceCreateInfo
  : OpenKneeboard::Vulkan::ExtendedCreateInfo<VkInstanceCreateInfo> {
  InstanceCreateInfo(const VkInstanceCreateInfo& base);
};

struct DeviceCreateInfo
  : OpenKneeboard::Vulkan::ExtendedCreateInfo<VkDeviceCreateInfo> {
  DeviceCreateInfo(const VkDeviceCreateInfo& base);

 private:
  VkPhysicalDeviceTimelineSemaphoreFeaturesKHR mTimelineSemaphores {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR};
};

class CachedReader : public SHM::CachedReader, protected SHM::IPCTextureCopier {
 public:
  CachedReader() = delete;
  CachedReader(ConsumerKind);
  virtual ~CachedReader();

  void InitializeCache(
    OpenKneeboard::Vulkan::Dispatch*,
    VkDevice,
    VkPhysicalDevice,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    const VkAllocationCallbacks*,
    uint8_t swapchainLength);

  static constexpr std::string_view REQUIRED_DEVICE_EXTENSIONS[] {
    VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
  };

  static constexpr std::string_view REQUIRED_INSTANCE_EXTENSIONS[] {
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

 protected:
  virtual void Copy(
    HANDLE sourceTexture,
    IPCClientTexture* destinationTexture,
    HANDLE fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept override;

  virtual std::shared_ptr<SHM::IPCClientTexture> CreateIPCClientTexture(
    const PixelSize&,
    uint8_t swapchainIndex) noexcept override;

 private:
  OpenKneeboard::Vulkan::Dispatch* mVK {nullptr};
  VkDevice mDevice {};
  VkPhysicalDevice mPhysicalDevice {};
  VkQueue mQueue {};
  uint32_t mQueueFamilyIndex {};

  const VkAllocationCallbacks* mAllocator {nullptr};

  unique_vk<VkCommandPool> mCommandPool;
  std::vector<VkCommandBuffer> mCommandBuffers;
  std::vector<unique_vk<VkFence>> mCompletionFences;

  std::unordered_map<HANDLE, unique_vk<VkSemaphore>> mIPCSemaphores;
  VkSemaphore GetIPCSemaphore(HANDLE);

  struct IPCImage {
    unique_vk<VkDeviceMemory> mMemory;
    unique_vk<VkImage> mImage;
    PixelSize mDimensions;
  };
  std::unordered_map<HANDLE, IPCImage> mIPCImages;
  VkImage GetIPCImage(HANDLE, const PixelSize&);
};

};// namespace OpenKneeboard::SHM::Vulkan