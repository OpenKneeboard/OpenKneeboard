// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/RenderDoc.hpp>
#include <OpenKneeboard/SHM/Vulkan.hpp>
#include <OpenKneeboard/Vulkan.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

namespace OpenKneeboard::SHM::Vulkan {

namespace {
[[nodiscard]]
uint64_t GetGPULuid(
  OpenKneeboard::Vulkan::Dispatch* const vk,
  const VkPhysicalDevice physicalDevice) {
  VkPhysicalDeviceIDProperties idProps {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR,
  };
  VkPhysicalDeviceProperties2KHR properties2 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
    .pNext = &idProps,
  };
  vk->GetPhysicalDeviceProperties2KHR(physicalDevice, &properties2);
  if (!idProps.deviceLUIDValid) {
    return std::numeric_limits<uint64_t>::max();
  }
  return std::bit_cast<uint64_t>(idProps.deviceLUID);
}
}// namespace

using OpenKneeboard::Vulkan::check_vkresult;

Reader::Reader(
  const ConsumerKind kind,
  OpenKneeboard::Vulkan::Dispatch* dispatch,
  VkInstance instance,
  VkDevice device,
  VkPhysicalDevice physicalDevice,
  uint32_t queueFamilyIndex,
  const VkAllocationCallbacks* allocator)
  : SHM::Reader(kind, GetGPULuid(dispatch, physicalDevice)),
    mVK(dispatch),
    mInstance(instance),
    mDevice(device),
    mPhysicalDevice(physicalDevice),
    mQueueFamilyIndex(queueFamilyIndex),
    mAllocator(allocator) {
}

Reader::~Reader() = default;

std::expected<Frame, Frame::Error> Reader::MaybeGetMapped() {
  auto raw = MaybeGet();
  if (!raw) {
    return std::unexpected {raw.error()};
  }
  return Map(*std::move(raw));
}

Frame Reader::Map(SHM::Frame raw) {
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::Reader::Map");
  auto& vk = mFrames.at(raw.mIndex);
  if (vk.mImageHandle != raw.mTexture) {
    vk.mImageHandle = raw.mTexture;
    vk.mImage.reset();
    vk.mImageView.reset();
    vk.mDimensions = {};
  }
  if (vk.mSemaphoreHandle != raw.mFence) {
    vk.mSemaphoreHandle = raw.mFence;
    vk.mSemaphore.reset();
  }

  if (!vk.mImage) {
    vk.mDimensions = raw.mConfig.mTextureSize;

    VkExternalMemoryImageCreateInfoKHR externalCreateInfo {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
    };

    static_assert(
      SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
    VkImageCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = &externalCreateInfo,
      .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .extent = {vk.mDimensions.mWidth, vk.mDimensions.mHeight, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &mQueueFamilyIndex,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vk.mImage = mVK->make_unique<VkImage>(mDevice, &createInfo, mAllocator);

    VkImageMemoryRequirementsInfo2KHR memoryInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
      .image = vk.mImage.get(),
    };

    VkMemoryRequirements2KHR memoryRequirements {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
    };

    mVK->GetImageMemoryRequirements2KHR(
      mDevice, &memoryInfo, &memoryRequirements);

    VkMemoryWin32HandlePropertiesKHR handleProperties {
      .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
    };

    check_vkresult(mVK->GetMemoryWin32HandlePropertiesKHR(
      mDevice,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
      vk.mImageHandle,
      &handleProperties));

    const auto memoryType = OpenKneeboard::Vulkan::FindMemoryType(
      mVK,
      mPhysicalDevice,
      handleProperties.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!memoryType) [[unlikely]] {
      fatal("Unable to find suitable memoryType");
    }

    VkImportMemoryWin32HandleInfoKHR importInfo {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
      .handle = vk.mImageHandle,
    };

    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
      .pNext = &importInfo,
      .image = vk.mImage.get(),
    };

    VkMemoryAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &dedicatedAllocInfo,
      .allocationSize = memoryRequirements.memoryRequirements.size,
      .memoryTypeIndex = *memoryType,
    };

    vk.mMemory
      = mVK->make_unique<VkDeviceMemory>(mDevice, &allocInfo, mAllocator);

    VkBindImageMemoryInfoKHR bindInfo {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
      .image = vk.mImage.get(),
      .memory = vk.mMemory.get(),
    };

    check_vkresult(mVK->BindImageMemory2KHR(mDevice, 1, &bindInfo));
  }

  if (!vk.mImageView) {
    VkImageViewCreateInfo viewCreateInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = vk.mImage.get(),
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    };

    vk.mImageView
      = mVK->make_unique<VkImageView>(mDevice, &viewCreateInfo, mAllocator);
  }

  if (!vk.mSemaphore) {
    VkSemaphoreTypeCreateInfoKHR typeCreateInfo {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
    };

    VkSemaphoreCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &typeCreateInfo,
    };

    vk.mSemaphore
      = mVK->make_unique<VkSemaphore>(mDevice, &createInfo, mAllocator);

    VkImportSemaphoreWin32HandleInfoKHR importInfo {
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .semaphore = vk.mSemaphore.get(),
      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
      .handle = vk.mSemaphoreHandle,
    };

    check_vkresult(mVK->ImportSemaphoreWin32HandleKHR(mDevice, &importInfo));
  }

  return Frame {
    .mConfig = std::move(raw.mConfig),
    .mLayers = std::move(raw.mLayers),
    .mImage = vk.mImage.get(),
    .mImageView = vk.mImageView.get(),
    .mDimensions = vk.mDimensions,
    .mSemaphore = vk.mSemaphore.get(),
    .mSemaphoreIn = std::bit_cast<uint64_t>(raw.mFenceIn),
  };
}

void Reader::OnSessionChanged() {
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::Reader::OnSessionChanged");
  mFrames = {};
}

InstanceCreateInfo::InstanceCreateInfo(const VkInstanceCreateInfo& base)
  : ExtendedCreateInfo(base, Reader::REQUIRED_INSTANCE_EXTENSIONS) {
}

DeviceCreateInfo::DeviceCreateInfo(const VkDeviceCreateInfo& base)
  : ExtendedCreateInfo(base, Reader::REQUIRED_DEVICE_EXTENSIONS) {
  bool enabledTimelineSemaphores = false;

  for (auto next = reinterpret_cast<const VkBaseOutStructure*>(pNext); next;
       next = next->pNext) {
    auto mut = const_cast<VkBaseOutStructure*>(next);
    switch (next->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES: {
        auto it = reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(mut);
        it->timelineSemaphore = true;
        enabledTimelineSemaphores = true;
        continue;
      }
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR: {
        auto it
          = reinterpret_cast<VkPhysicalDeviceTimelineSemaphoreFeaturesKHR*>(
            mut);
        it->timelineSemaphore = true;
        enabledTimelineSemaphores = true;
        continue;
      }
      default:
        continue;
    }
  }

  if (!enabledTimelineSemaphores) {
    auto it = &mTimelineSemaphores;
    it->timelineSemaphore = true;
    it->pNext = const_cast<void*>(pNext);
    pNext = it;
  }
}

}// namespace OpenKneeboard::SHM::Vulkan