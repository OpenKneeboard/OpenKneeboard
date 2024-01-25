/*
 * OpenKneeboard
 *
 * Copyright (C) 2022-present Fred Emmott <fred@fredemmott.com>
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

#include <OpenKneeboard/SHM/Vulkan.h>
#include <OpenKneeboard/Vulkan.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/tracing.h>

namespace OpenKneeboard::SHM::Vulkan {

using OpenKneeboard::Vulkan::check_vkresult;

Texture::Texture(const PixelSize& dimensions, uint8_t swapchainIndex)
  : IPCClientTexture(dimensions, swapchainIndex) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::Texture::Texture()");
}

Texture::~Texture() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::Texture::~Texture()");
}

VkImage Texture::GetVKImage() {
  return mSourceImage.get();
}

VkImageView Texture::GetVKRenderTargetView() {
  return mRenderTargetView.get();
}

void Texture::CopyFrom(
  OpenKneeboard::Vulkan::Dispatch* vk,
  VkDevice device,
  VkPhysicalDevice physicalDevice,
  VkQueue queue,
  uint32_t queueFamilyIndex,
  const VkAllocationCallbacks* allocator,
  VkCommandBuffer commandBuffer,
  HANDLE texture,
  HANDLE semaphore,
  uint64_t semaphoreValueIn,
  uint64_t semaphoreValueOut,
  VkFence completionFence) noexcept {
  this->InitializeImages(
    vk, device, physicalDevice, queueFamilyIndex, allocator, texture);
  this->InitializeSemaphore(vk, device, allocator, semaphore);

  VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  check_vkresult(vk->BeginCommandBuffer(commandBuffer, &beginInfo));

  const auto dimensions = this->GetDimensions();
  VkImageCopy regions[] { 
    VkImageCopy {
      .srcSubresource = VkImageSubresourceLayers {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .layerCount = 1,
      },
      .dstSubresource = VkImageSubresourceLayers {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .layerCount = 1,
      },
      .extent = { dimensions.mWidth, dimensions.mHeight, 1 },
    },
  };

  const auto srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  const auto destLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  std::vector<VkImageMemoryBarrier> inBarriers {
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = srcLayout,
      .image = mSourceImage.get(),
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    },
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = mImageLayout,
      .newLayout = destLayout,
      .image = mImage.get(),
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    },
  };

  vk->CmdPipelineBarrier(
    commandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    static_cast<uint32_t>(inBarriers.size()),
    inBarriers.data());

  vk->CmdCopyImage(
    commandBuffer,
    mSourceImage.get(),
    srcLayout,
    mImage.get(),
    destLayout,
    std::size(regions),
    regions);

  check_vkresult(vk->EndCommandBuffer(commandBuffer));

  VkTimelineSemaphoreSubmitInfoKHR semaphoreInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
    .waitSemaphoreValueCount = 1,
    .pWaitSemaphoreValues = &semaphoreValueIn,
    .signalSemaphoreValueCount = 1,
    .pSignalSemaphoreValues = &semaphoreValueOut,
  };

  // TODO: is COPY_BIT enough?
  VkPipelineStageFlags semaphoreStages {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};

  VkSemaphore semaphores[] = {mSemaphore.get()};

  VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &semaphoreInfo,
    .waitSemaphoreCount = std::size(semaphores),
    .pWaitSemaphores = semaphores,
    .pWaitDstStageMask = &semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &commandBuffer,
    .signalSemaphoreCount = std::size(semaphores),
    .pSignalSemaphores = semaphores,
  };

  check_vkresult(vk->QueueSubmit(queue, 1, &submitInfo, completionFence));
}

void Texture::InitializeImages(
  OpenKneeboard::Vulkan::Dispatch* vk,
  VkDevice device,
  VkPhysicalDevice physicalDevice,
  uint32_t queueFamilyIndex,
  const VkAllocationCallbacks* allocator,
  HANDLE imageHandle) {
  if (imageHandle == mSourceImageHandle) {
    return;
  }
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::Texture::InitializeImages()");

  const auto dimensions = this->GetDimensions();

  VkExternalMemoryImageCreateInfoKHR externalCreateInfo {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
  };

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  // TODO: Using VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT here because - like all the
  // other renderers - I'm using an SRGB view on a UNORM texture. That gets good
  // results, but probably means I'm messing something up earlier.
  VkImageCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = &externalCreateInfo,
    .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_B8G8R8A8_UNORM,
    .extent = {dimensions.mWidth, dimensions.mHeight, 1},
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = &queueFamilyIndex,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  mSourceImage = vk->make_unique<VkImage>(device, &createInfo, allocator);
  if (!mImage) {
    createInfo.pNext = nullptr;
    createInfo.usage
      = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    mImage = vk->make_unique<VkImage>(device, &createInfo, allocator);
    mRenderTargetView = {};
    mImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  }

  VkImageMemoryRequirementsInfo2KHR memoryInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
    .image = mSourceImage.get(),
  };

  VkMemoryRequirements2KHR memoryRequirements {
    .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
  };

  vk->GetImageMemoryRequirements2KHR(device, &memoryInfo, &memoryRequirements);

  VkMemoryWin32HandlePropertiesKHR handleProperties {
    .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
  };

  check_vkresult(vk->GetMemoryWin32HandlePropertiesKHR(
    device,
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
    imageHandle,
    &handleProperties));

  const auto memoryType = OpenKneeboard::Vulkan::FindMemoryType(
    vk,
    physicalDevice,
    handleProperties.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (!memoryType) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Unable to find suitable memoryType");
  }

  VkImportMemoryWin32HandleInfoKHR importInfo {
    .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
    .handle = imageHandle,
  };

  VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    .pNext = &importInfo,
    .image = mSourceImage.get(),
  };

  VkMemoryAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &dedicatedAllocInfo,
    .allocationSize = memoryRequirements.memoryRequirements.size,
    .memoryTypeIndex = *memoryType,
  };

  mSourceImageMemory
    = vk->make_unique<VkDeviceMemory>(device, &allocInfo, allocator);
  if (!mImageMemory) {
    dedicatedAllocInfo.pNext = nullptr;
    dedicatedAllocInfo.image = mImage.get();
    mImageMemory
      = vk->make_unique<VkDeviceMemory>(device, &allocInfo, allocator);
  }

  VkBindImageMemoryInfoKHR bindInfo {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
    .image = mSourceImage.get(),
    .memory = mSourceImageMemory.get(),
  };

  check_vkresult(vk->BindImageMemory2KHR(device, 1, &bindInfo));
  if (!mRenderTargetView) {
    bindInfo.image = mImage.get();
    bindInfo.memory = mImageMemory.get();
    check_vkresult(vk->BindImageMemory2KHR(device, 1, &bindInfo));

    VkImageViewCreateInfo viewCreateInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = mImage.get(),
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_SRGB,
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    };

    mRenderTargetView
      = vk->make_unique<VkImageView>(device, &viewCreateInfo, allocator);
  }

  mSourceImageHandle = imageHandle;
}

void Texture::InitializeSemaphore(
  OpenKneeboard::Vulkan::Dispatch* vk,
  VkDevice device,
  const VkAllocationCallbacks* allocator,
  HANDLE fenceHandle) {
  if (fenceHandle == mSemaphoreHandle) {
    return;
  }
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::Texture::InitializeFence");

  VkSemaphoreTypeCreateInfoKHR typeCreateInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
  };

  VkSemaphoreCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &typeCreateInfo,
  };

  mSemaphore = vk->make_unique<VkSemaphore>(device, &createInfo, allocator);

  VkImportSemaphoreWin32HandleInfoKHR importInfo {
    .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
    .semaphore = mSemaphore.get(),
    .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
    .handle = fenceHandle,
  };

  check_vkresult(vk->ImportSemaphoreWin32HandleKHR(device, &importInfo));

  mSemaphoreHandle = fenceHandle;
}

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::~CachedReader()");

  if (!mCompletionFences.empty()) {
    std::vector<VkFence> fences;
    for (const auto& fence: mCompletionFences) {
      fences.push_back(fence.get());
    }
    check_vkresult(mVK->WaitForFences(
      mDevice, fences.size(), fences.data(), true, ~(0ui64)));
  }

  mVK->FreeCommandBuffers(
    mDevice,
    mCommandPool.get(),
    static_cast<uint32_t>(mCommandBuffers.size()),
    mCommandBuffers.data());

  dprint("hitting autos");
}

void CachedReader::InitializeCache(
  OpenKneeboard::Vulkan::Dispatch* dispatch,
  VkDevice device,
  VkPhysicalDevice physicalDevice,
  uint32_t queueFamilyIndex,
  uint32_t queueIndex,
  const VkAllocationCallbacks* allocator,
  uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "swapchainLength"));

  mVK = dispatch;

  mDevice = device;
  mPhysicalDevice = physicalDevice;
  mAllocator = allocator;

  if (!mCompletionFences.empty()) {
    std::vector<VkFence> fences;
    for (const auto& fence: mCompletionFences) {
      fences.push_back(fence.get());
    }
    mVK->WaitForFences(device, fences.size(), fences.data(), true, ~(0ui64));
  }

  mQueueFamilyIndex = queueFamilyIndex;
  dispatch->GetDeviceQueue(device, queueFamilyIndex, queueIndex, &mQueue);

  {
    VkCommandPoolCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndex,
    };
    mCommandPool
      = mVK->make_unique<VkCommandPool>(mDevice, &createInfo, mAllocator);
  }

  mCommandBuffers = {swapchainLength, nullptr};
  {
    VkCommandBufferAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = mCommandPool.get(),
      .commandBufferCount = swapchainLength,
    };
    check_vkresult(
      mVK->AllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()));
  }

  {
    VkFenceCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    mCompletionFences.clear();
    for (uint8_t i = 0; i < swapchainLength; ++i) {
      mCompletionFences.push_back(
        mVK->make_unique<VkFence>(mDevice, &createInfo, mAllocator));
    };
  }

  SHM::CachedReader::InitializeCache(swapchainLength);
}

void CachedReader::Copy(
  HANDLE sourceTexture,
  IPCClientTexture* destinationTexture,
  HANDLE semaphore,
  uint64_t semaphoreValueIn,
  uint64_t semaphoreValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::Copy()");

  const auto swapchainIndex = destinationTexture->GetSwapchainIndex();

  auto fence = mCompletionFences.at(swapchainIndex).get();
  check_vkresult(mVK->WaitForFences(mDevice, 1, &fence, true, ~(0ui64)));
  check_vkresult(mVK->ResetFences(mDevice, 1, &fence));

  reinterpret_cast<Texture*>(destinationTexture)
    ->CopyFrom(
      mVK,
      mDevice,
      mPhysicalDevice,
      mQueue,
      mQueueFamilyIndex,
      mAllocator,
      mCommandBuffers.at(swapchainIndex),
      sourceTexture,
      semaphore,
      semaphoreValueIn,
      semaphoreValueOut,
      fence);
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<Texture>(dimensions, swapchainIndex);
}

InstanceCreateInfo::InstanceCreateInfo(const VkInstanceCreateInfo& base)
  : ExtendedCreateInfo(base, CachedReader::REQUIRED_INSTANCE_EXTENSIONS) {
}

DeviceCreateInfo::DeviceCreateInfo(const VkDeviceCreateInfo& base)
  : ExtendedCreateInfo(base, CachedReader::REQUIRED_DEVICE_EXTENSIONS) {
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
    }
  }

  if (!enabledTimelineSemaphores) {
    auto it = &mTimelineSemaphores;
    it->timelineSemaphore = true;
    it->pNext = const_cast<void*>(pNext);
    pNext = it;
  }
}

};// namespace OpenKneeboard::SHM::Vulkan