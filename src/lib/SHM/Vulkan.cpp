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

Texture::Texture() {
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

PixelSize Texture::GetDimensions() const {
  return mDimensions;
}

void Texture::CopyFrom(
  OpenKneeboard::Vulkan::Dispatch* vk,
  VkDevice device,
  VkPhysicalDevice physicalDevice,
  uint32_t queueFamilyIndex,
  const VkAllocationCallbacks* allocator,
  VkQueue commandQueue,
  VkCommandBuffer commandBuffer,
  HANDLE texture,
  const PixelSize& textureDimensions,
  HANDLE fence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  this->InitializeSource(
    vk,
    device,
    physicalDevice,
    queueFamilyIndex,
    allocator,
    texture,
    textureDimensions);
  this->InitializeFence(vk, device, allocator, fence);
}

void Texture::InitializeSource(
  OpenKneeboard::Vulkan::Dispatch* vk,
  VkDevice device,
  VkPhysicalDevice physicalDevice,
  uint32_t queueFamilyIndex,
  const VkAllocationCallbacks* allocator,
  HANDLE imageHandle,
  const PixelSize& dimensions) {
  if (dimensions != mSourceDimensions) {
    mSourceImageHandle = {};
  }
  if (imageHandle == mSourceImageHandle) {
    return;
  }
  OPENKNEEBOARD_TraceLoggingScope("Vulkan::Texture::InitializeFence");

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  VkImageCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .format = VK_FORMAT_B8G8R8A8_UNORM,
    .extent = {dimensions.mWidth, dimensions.mHeight},
    .mipLevels = 1,
    .arrayLayers = 1,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage
    = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = &queueFamilyIndex,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  mSourceImage = vk->make_unique<VkImage>(device, &createInfo, allocator);

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

  VkBindImageMemoryInfoKHR bindInfo {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
    .image = mSourceImage.get(),
    .memory = mSourceImageMemory.get(),
  };

  check_vkresult(vk->BindImageMemory2KHR(device, 1, &bindInfo));

  mSourceDimensions = dimensions;
  mSourceImageHandle = imageHandle;
}

void Texture::InitializeFence(
  OpenKneeboard::Vulkan::Dispatch* vk,
  VkDevice device,
  const VkAllocationCallbacks* allocator,
  HANDLE fenceHandle) {
  if (fenceHandle == mFenceHandle) {
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

  mFence = vk->make_unique<VkSemaphore>(device, &createInfo, allocator);

  VkImportSemaphoreWin32HandleInfoKHR importInfo {
    .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
    .semaphore = mFence.get(),
    .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
    .handle = fenceHandle,
  };

  check_vkresult(vk->ImportSemaphoreWin32HandleKHR(device, &importInfo));

  mFenceHandle = fenceHandle;
}

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::~CachedReader()");
}

void CachedReader::InitializeCache(
  OpenKneeboard::Vulkan::Dispatch* dispatch,
  VkDevice device,
  uint32_t queueFamilyIndex,
  [[maybe_unused]] uint32_t queueFamily,
  const VkAllocationCallbacks* allocator,
  uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "swapchainLength"));
  mVK = dispatch;
  mDevice = device;
  mAllocator = allocator;

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

  SHM::CachedReader::InitializeCache(swapchainLength);
}

void CachedReader::Copy(
  uint8_t swapchainIndex,
  HANDLE sourceTexture,
  IPCClientTexture* destinationTexture,
  HANDLE fence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::Vulkan::CachedReader::Copy()");
  // TODO
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::Vulkan::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<Texture>();
}
};// namespace OpenKneeboard::SHM::Vulkan