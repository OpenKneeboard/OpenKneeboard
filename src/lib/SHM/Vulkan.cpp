// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/RenderDoc.hpp>
#include <OpenKneeboard/SHM/Vulkan.hpp>
#include <OpenKneeboard/Vulkan.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

namespace OpenKneeboard::SHM::Vulkan {

using OpenKneeboard::Vulkan::check_vkresult;

Texture::Texture(
  OpenKneeboard::Vulkan::Dispatch* vk,
  VkPhysicalDevice physicalDevice,
  VkDevice device,
  uint32_t queueFamilyIndex,
  const VkAllocationCallbacks* allocator,
  VkFence completionFence,
  const PixelSize& dimensions,
  uint8_t swapchainIndex)
  : IPCClientTexture(dimensions, swapchainIndex),
    mVK(vk),
    mPhysicalDevice(physicalDevice),
    mDevice(device),
    mQueueFamilyIndex(queueFamilyIndex),
    mAllocator(allocator),
    mCompletionFence(completionFence) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::Texture::Texture()");
  this->InitializeCacheImage();
  this->InitializeReadySemaphore();
}

Texture::~Texture() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::Texture::~Texture()");

  // We can't tear down our semaphore while it's pending
  VkSemaphore semaphores[] {mReadySemaphore.get()};
  uint64_t semaphoreValues[] {mReadySemaphoreValue};

  VkSemaphoreWaitInfoKHR waitInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
    .semaphoreCount = std::size(semaphores),
    .pSemaphores = semaphores,
    .pValues = semaphoreValues,
  };
  check_vkresult(mVK->WaitSemaphoresKHR(mDevice, &waitInfo, ~(0ui64)));

  // ... or while a batch referring to it is in progress:
  check_vkresult(
    mVK->WaitForFences(mDevice, 1, &mCompletionFence, 1, ~(0ui64)));
}

VkImage Texture::GetVKImage() {
  return mImage.get();
}

VkImageView Texture::GetVKImageView() {
  return mImageView.get();
}

VkSemaphore Texture::GetReadySemaphore() const {
  return mReadySemaphore.get();
}

uint64_t Texture::GetReadySemaphoreValue() const {
  return mReadySemaphoreValue;
}

void Texture::CopyFrom(
  VkQueue queue,
  VkCommandBuffer commandBuffer,
  VkImage source,
  VkSemaphore semaphore,
  uint64_t semaphoreValueIn) noexcept {
  VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  check_vkresult(mVK->BeginCommandBuffer(commandBuffer, &beginInfo));

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

  const auto dest = mImage.get();

  VkImageMemoryBarrier inBarriers[] {
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .image = source,
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    },
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .image = dest,
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    },
  };

  mVK->CmdPipelineBarrier(
    commandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    std::size(inBarriers),
    inBarriers);

  mVK->CmdCopyImage(
    commandBuffer,
    source,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    dest,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    std::size(regions),
    regions);

  VkImageMemoryBarrier outBarriers[] {
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .image = dest,
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    },
  };
  mVK->CmdPipelineBarrier(
    commandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    std::size(outBarriers),
    outBarriers);

  check_vkresult(mVK->EndCommandBuffer(commandBuffer));

  VkSemaphore waitSemaphores[] = {semaphore};
  uint64_t waitSemaphoreValues[] = {semaphoreValueIn};

  VkSemaphore signalSemaphores[] = {
    mReadySemaphore.get(),
  };
  const uint64_t signalSemaphoreValues[] = {
    ++mReadySemaphoreValue,
  };

  VkTimelineSemaphoreSubmitInfoKHR semaphoreInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
    .waitSemaphoreValueCount = std::size(waitSemaphoreValues),
    .pWaitSemaphoreValues = waitSemaphoreValues,
    .signalSemaphoreValueCount = std::size(signalSemaphoreValues),
    .pSignalSemaphoreValues = signalSemaphoreValues,
  };

  // TODO: is COPY_BIT enough?
  VkPipelineStageFlags semaphoreStages {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};

  VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &semaphoreInfo,
    .waitSemaphoreCount = std::size(waitSemaphores),
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = &semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &commandBuffer,
    .signalSemaphoreCount = std::size(signalSemaphores),
    .pSignalSemaphores = signalSemaphores,
  };

  check_vkresult(mVK->QueueSubmit(queue, 1, &submitInfo, mCompletionFence));
}

void Texture::InitializeCacheImage() {
  if (mImage) [[unlikely]] {
    fatal("Double-initializing cache image");
  }
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::Texture::InitializeCacheImages()");

  const auto dimensions = this->GetDimensions();

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  VkImageCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_B8G8R8A8_UNORM,
    .extent = {dimensions.mWidth, dimensions.mHeight, 1},
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = &mQueueFamilyIndex,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  mImage = mVK->make_unique<VkImage>(mDevice, &createInfo, mAllocator);

  VkMemoryRequirements memoryRequirements {};
  mVK->GetImageMemoryRequirements(mDevice, mImage.get(), &memoryRequirements);

  const auto memoryType = OpenKneeboard::Vulkan::FindMemoryType(
    mVK,
    mPhysicalDevice,
    memoryRequirements.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (!memoryType) [[unlikely]] {
    fatal("Unable to find suitable memoryType");
  }

  VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    .image = mImage.get(),
  };

  VkMemoryAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &dedicatedAllocInfo,
    .allocationSize = memoryRequirements.size,
    .memoryTypeIndex = *memoryType,
  };

  mImageMemory
    = mVK->make_unique<VkDeviceMemory>(mDevice, &allocInfo, mAllocator);

  VkBindImageMemoryInfoKHR bindInfo {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
    .image = mImage.get(),
    .memory = mImageMemory.get(),
  };

  check_vkresult(mVK->BindImageMemory2KHR(mDevice, 1, &bindInfo));

  VkImageViewCreateInfo viewCreateInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = mImage.get(),
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    };

  mImageView
    = mVK->make_unique<VkImageView>(mDevice, &viewCreateInfo, mAllocator);
}

void Texture::InitializeReadySemaphore() {
  if (mReadySemaphore) [[unlikely]] {
    // destructor assumes this was called by destructor
    fatal("Double-initializing semaphore");
  }

  OPENKNEEBOARD_TraceLoggingScope("Vulkan::Texture::InitializeReadySemaphore");

  VkSemaphoreTypeCreateInfoKHR typeCreateInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
  };

  VkSemaphoreCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &typeCreateInfo,
  };

  mReadySemaphore
    = mVK->make_unique<VkSemaphore>(mDevice, &createInfo, mAllocator);
}

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::~CachedReader()");

  this->ReleaseIPCHandles();

  this->InitializeCache(0);

  mVK->FreeCommandBuffers(
    mDevice,
    mCommandPool.get(),
    static_cast<uint32_t>(mCommandBuffers.size()),
    mCommandBuffers.data());
}

void CachedReader::InitializeCache(
  OpenKneeboard::Vulkan::Dispatch* dispatch,
  VkInstance instance,
  VkDevice device,
  VkPhysicalDevice physicalDevice,
  uint32_t queueFamilyIndex,
  uint32_t queueIndex,
  const VkAllocationCallbacks* allocator,
  uint8_t swapchainLength) {
  mVK = dispatch;

  mInstance = instance;
  mDevice = device;
  mPhysicalDevice = physicalDevice;
  mAllocator = allocator;
  mQueueFamilyIndex = queueFamilyIndex;

  mVK->GetDeviceQueue(mDevice, queueFamilyIndex, queueIndex, &mQueue);

  VkPhysicalDeviceIDPropertiesKHR id {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR,
  };
  VkPhysicalDeviceProperties2KHR properties2 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
    .pNext = &id,
  };
  mVK->GetPhysicalDeviceProperties2KHR(mPhysicalDevice, &properties2);

  if (!id.deviceLUIDValid) {
    fatal("Could not retrieve a device LUID");
  }
  static_assert(sizeof(uint64_t) == sizeof(id.deviceLUID));
  mGPULUID = std::bit_cast<uint64_t>(id.deviceLUID);

  dprint(
    "Vulkan SHM reader using adapter '{}' (LUID {:#x})",
    properties2.properties.deviceName,
    mGPULUID);

  this->InitializeCache(swapchainLength);
}

void CachedReader::InitializeCache(uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "swapchainLength"));

  this->WaitForAllFences();

  if (swapchainLength == 0) {
    mIPCSemaphores.clear();
    mIPCImages.clear();
    SHM::CachedReader::InitializeCache(mGPULUID, 0);
    return;
  }

  if (!mCommandPool) {
    VkCommandPoolCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = mQueueFamilyIndex,
    };
    mCommandPool
      = mVK->make_unique<VkCommandPool>(mDevice, &createInfo, mAllocator);
  }

  if (swapchainLength > mCommandBuffers.size()) {
    const auto oldLength = mCommandBuffers.size();
    VkCommandBufferAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = mCommandPool.get(),
      .commandBufferCount = static_cast<uint32_t>(swapchainLength - oldLength),
    };
    mCommandBuffers.resize(swapchainLength);
    check_vkresult(mVK->AllocateCommandBuffers(
      mDevice, &allocInfo, &mCommandBuffers.at(oldLength)));
  }

  {
    VkFenceCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (uint8_t i = mCompletionFences.size(); i < swapchainLength; ++i) {
      mCompletionFences.push_back(
        mVK->make_unique<VkFence>(mDevice, &createInfo, mAllocator));
    };
  }

  mIPCSemaphores.clear();
  mIPCImages.clear();

  SHM::CachedReader::InitializeCache(mGPULUID, swapchainLength);
}

void CachedReader::Copy(
  HANDLE sourceHandle,
  IPCClientTexture* destinationTexture,
  HANDLE semaphoreHandle,
  uint64_t semaphoreValueIn) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::Copy()");

  const auto swapchainIndex = destinationTexture->GetSwapchainIndex();

  auto dimensions = destinationTexture->GetDimensions();

  auto source = this->GetIPCImage(sourceHandle, dimensions);
  auto semaphore = this->GetIPCSemaphore(semaphoreHandle);

  auto fence = mCompletionFences.at(swapchainIndex).get();
  check_vkresult(mVK->WaitForFences(mDevice, 1, &fence, true, ~(0ui64)));
  check_vkresult(mVK->ResetFences(mDevice, 1, &fence));

  reinterpret_cast<Texture*>(destinationTexture)
    ->CopyFrom(
      mQueue,
      mCommandBuffers.at(swapchainIndex),
      source,
      semaphore,
      semaphoreValueIn);
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::CreateIPCClientTexture()");

  return std::make_shared<Texture>(
    mVK,
    mPhysicalDevice,
    mDevice,
    mQueueFamilyIndex,
    mAllocator,
    mCompletionFences.at(swapchainIndex).get(),
    dimensions,
    swapchainIndex);
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

VkSemaphore CachedReader::GetIPCSemaphore(HANDLE handle) {
  if (mIPCSemaphores.contains(handle)) {
    return mIPCSemaphores.at(handle).get();
  }
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::CachedReader::GetIPCSemaphore");

  VkSemaphoreTypeCreateInfoKHR typeCreateInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
  };

  VkSemaphoreCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &typeCreateInfo,
  };

  auto uniqueSemaphore
    = mVK->make_unique<VkSemaphore>(mDevice, &createInfo, mAllocator);

  VkImportSemaphoreWin32HandleInfoKHR importInfo {
    .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
    .semaphore = uniqueSemaphore.get(),
    .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
    .handle = handle,
  };

  check_vkresult(mVK->ImportSemaphoreWin32HandleKHR(mDevice, &importInfo));

  const auto ret = uniqueSemaphore.get();
  mIPCSemaphores.emplace(handle, std::move(uniqueSemaphore));
  return ret;
}

VkImage CachedReader::GetIPCImage(HANDLE handle, const PixelSize& dimensions) {
  if (mIPCImages.contains(handle)) {
    const auto& data = mIPCImages.at(handle);
    if (data.mDimensions != dimensions) [[unlikely]] {
      fatal("Reported dimensions of image handle have changed");
    }
    return data.mImage.get();
  }
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::CachedReader::GetIPCImage()");

  VkExternalMemoryImageCreateInfoKHR externalCreateInfo {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
  };

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
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
    .pQueueFamilyIndices = &mQueueFamilyIndex,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  auto image = mVK->make_unique<VkImage>(mDevice, &createInfo, mAllocator);

  VkImageMemoryRequirementsInfo2KHR memoryInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
    .image = image.get(),
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
    handle,
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
    .handle = handle,
  };

  VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    .pNext = &importInfo,
    .image = image.get(),
  };

  VkMemoryAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &dedicatedAllocInfo,
    .allocationSize = memoryRequirements.memoryRequirements.size,
    .memoryTypeIndex = *memoryType,
  };

  auto memory
    = mVK->make_unique<VkDeviceMemory>(mDevice, &allocInfo, mAllocator);

  VkBindImageMemoryInfoKHR bindInfo {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
    .image = image.get(),
    .memory = memory.get(),
  };

  check_vkresult(mVK->BindImageMemory2KHR(mDevice, 1, &bindInfo));

  const auto ret = image.get();
  mIPCImages.emplace(
    handle, IPCImage {std::move(memory), std::move(image), dimensions});
  return ret;
}

void CachedReader::ReleaseIPCHandles() {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::ReleaseIPCHandles");

  this->WaitForAllFences();

  mIPCSemaphores.clear();
  mIPCImages.clear();
}

void CachedReader::WaitForAllFences() {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::WaitForAllFences");

  if (mCompletionFences.empty()) {
    return;
  }

  std::vector<VkFence> fences;
  for (const auto& fence: mCompletionFences) {
    fences.push_back(fence.get());
  }
  check_vkresult(
    mVK->WaitForFences(mDevice, fences.size(), fences.data(), true, ~(0ui64)));
}

Snapshot CachedReader::MaybeGet(const std::source_location& loc) {
  RenderDoc::NestedFrameCapture renderDocFrame(
    mInstance, "SHM::Vulkan::MaybeGet()");
  return SHM::CachedReader::MaybeGet(loc);
}

};// namespace OpenKneeboard::SHM::Vulkan