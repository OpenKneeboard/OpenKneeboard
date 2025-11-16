// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/Vulkan/Dispatch.hpp>
#include <OpenKneeboard/Vulkan/ExtendedCreateInfo.hpp>
#include <OpenKneeboard/Vulkan/smart-pointers.hpp>

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
    uint64_t sempahoreValueIn) noexcept;

 private:
  OpenKneeboard::Vulkan::Dispatch* mVK {nullptr};
  VkPhysicalDevice mPhysicalDevice {VK_NULL_HANDLE};
  VkDevice mDevice {VK_NULL_HANDLE};
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
    VkInstance,
    VkDevice,
    VkPhysicalDevice,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    const VkAllocationCallbacks*,
    uint8_t swapchainLength);

  virtual Snapshot MaybeGet(
    const std::source_location& loc = std::source_location::current()) override;

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
  void InitializeCache(uint8_t swapchainLength);
  virtual void Copy(
    HANDLE sourceTexture,
    IPCClientTexture* destinationTexture,
    HANDLE fence,
    uint64_t fenceValueIn) noexcept override;

  virtual std::shared_ptr<SHM::IPCClientTexture> CreateIPCClientTexture(
    const PixelSize&,
    uint8_t swapchainIndex) noexcept override;

  virtual void ReleaseIPCHandles() override;

 private:
  OpenKneeboard::Vulkan::Dispatch* mVK {nullptr};
  VkInstance mInstance {};
  VkDevice mDevice {};
  VkPhysicalDevice mPhysicalDevice {};
  VkQueue mQueue {};
  uint32_t mQueueFamilyIndex {};

  uint64_t mGPULUID {};

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

  void WaitForAllFences();
};

};// namespace OpenKneeboard::SHM::Vulkan