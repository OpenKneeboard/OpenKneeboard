// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/Vulkan/Dispatch.hpp>
#include <OpenKneeboard/Vulkan/ExtendedCreateInfo.hpp>
#include <OpenKneeboard/Vulkan/smart-pointers.hpp>

namespace OpenKneeboard::SHM::Vulkan {

using OpenKneeboard::Vulkan::unique_vk;

struct Frame {
  using Error = SHM::Frame::Error;
  Config mConfig {};
  decltype(SHM::Frame::mLayers) mLayers {};

  VkImage mImage {};
  VkImageView mImageView {};
  PixelSize mDimensions {};

  VkSemaphore mSemaphore {};
  uint64_t mSemaphoreIn {};
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

class Reader final : public SHM::Reader {
 public:
  Reader(
    ConsumerKind,
    OpenKneeboard::Vulkan::Dispatch*,
    VkInstance,
    VkDevice,
    VkPhysicalDevice,
    uint32_t queueFamilyIndex,
    const VkAllocationCallbacks*);
  ~Reader() override;

  Frame Map(SHM::Frame);
  std::expected<Frame, Frame::Error> MaybeGetMapped();

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
  void OnSessionChanged() override;

 private:
  struct FrameVulkanResources {
    HANDLE mImageHandle {};
    unique_vk<VkDeviceMemory> mMemory;
    unique_vk<VkImage> mImage;
    unique_vk<VkImageView> mImageView;
    PixelSize mDimensions {};

    HANDLE mSemaphoreHandle {};
    unique_vk<VkSemaphore> mSemaphore;
  };

  OpenKneeboard::Vulkan::Dispatch* mVK {nullptr};
  VkInstance mInstance {};
  VkDevice mDevice {};
  VkPhysicalDevice mPhysicalDevice {};
  uint32_t mQueueFamilyIndex {};

  const VkAllocationCallbacks* mAllocator {nullptr};

  std::array<FrameVulkanResources, SHM::SwapChainLength> mFrames {};
};

}// namespace OpenKneeboard::SHM::Vulkan
