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

#include "viewer-vulkan.h"

#include <OpenKneeboard/RenderDoc.h>
#include <OpenKneeboard/Vulkan.h>

#include <OpenKneeboard/dprint.h>

#include <DirectXTex.h>

// This is from the DirectXTex library, and doesn't touch the GPU
// or Direct3D at all
using DXCPUImage = DirectX::Image;

using OpenKneeboard::Vulkan::check_vkresult;

namespace OpenKneeboard::Viewer {

static constexpr std::array RequiredInstanceExtensions {
  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
  VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
#ifdef DEBUG
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

static VkBool32 VKDebugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData) {
  std::string_view severity;
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    severity = "ERROR";
  } else if (
    messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    severity = "WARNING";
  } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    severity = "info";
  } else if (
    messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    severity = "verbose";
  } else {
    severity = "MAGICNO";
    OPENKNEEBOARD_BREAK;
  }

  dprintf(
    "VK {} [{}]: {}",
    (pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "Debug"),
    severity,
    pCallbackData->pMessage);
  if (
    (!RenderDoc::IsPresent())
    && (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT))) {
    OPENKNEEBOARD_BREAK;
  }

  return VK_FALSE;
}

VulkanRenderer::VulkanRenderer(uint64_t luid) {
  mVulkanLoader = unique_hmodule {LoadLibraryA("vulkan-1.dll")};
  if (!mVulkanLoader) {
    OPENKNEEBOARD_LOG_AND_FATAL("Failed to load vulkan-1.dll");
  }
  auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
    GetProcAddress(mVulkanLoader.get(), "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddr) {
    OPENKNEEBOARD_LOG_AND_FATAL("Failed to find vkGetInstanceProcAddr");
  }

  auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
    vkGetInstanceProcAddr(NULL, "vkCreateInstance"));
  if (!vkCreateInstance) {
    OPENKNEEBOARD_LOG_AND_FATAL("Failed to find vkCreateInstance");
  }

  const VkApplicationInfo applicationInfo {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "OpenKneeboard-Viewer",
    .applicationVersion = 1,
    .apiVersion = VK_API_VERSION_1_0,
  };

#ifdef DEBUG
  dprint("Enabling Vulkan validation and debug messages");
  const VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = &VKDebugCallback,
  };
  const auto next = &debugCreateInfo;
#else
  constexpr nullptr_t next = nullptr;
#endif

  // Not doing a compile-time constant as it ends up as an empty
  // array in release builds, which is invalid.
  std::vector<const char*> requiredLayers;
#ifdef DEBUG
  requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  const Vulkan::CombinedCreateInfo<
    SHM::Vulkan::InstanceCreateInfo,
    Vulkan::SpriteBatch::InstanceCreateInfo>
    instanceCreateInfo(VkInstanceCreateInfo {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = next,
      .pApplicationInfo = &applicationInfo,
      .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
      .ppEnabledLayerNames = requiredLayers.data(),
      .enabledExtensionCount = RequiredInstanceExtensions.size(),
      .ppEnabledExtensionNames = RequiredInstanceExtensions.data(),
    });

  VkInstance instance;
  check_vkresult(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

  auto vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
    vkGetInstanceProcAddr(instance, "vkDestroyInstance"));

  if (!vkDestroyInstance) {
    OPENKNEEBOARD_LOG_AND_FATAL("Failed to find vkDestroyInstance");
  }
  mVKInstance = {instance, {vkDestroyInstance, nullptr}};

  mVK = std::make_unique<OpenKneeboard::Vulkan::Dispatch>(
    mVKInstance.get(), vkGetInstanceProcAddr);
#ifdef DEBUG
  mVKDebugMessenger = mVK->make_unique<VkDebugUtilsMessengerEXT>(
    mVKInstance.get(), &debugCreateInfo, nullptr);
#endif

  dprintf("Looking for GPU with LUID {:#018x}", luid);
  uint32_t physicalDeviceCount = 0;
  check_vkresult(mVK->EnumeratePhysicalDevices(
    mVKInstance.get(), &physicalDeviceCount, nullptr));
  std::vector<VkPhysicalDevice> physicalDevices {
    physicalDeviceCount, VK_NULL_HANDLE};
  check_vkresult(mVK->EnumeratePhysicalDevices(
    mVKInstance.get(), &physicalDeviceCount, physicalDevices.data()));

  for (const auto physicalDevice: physicalDevices) {
    VkPhysicalDeviceIDPropertiesKHR id {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR,
    };
    VkPhysicalDeviceProperties2KHR properties2 {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
      .pNext = &id,
    };
    mVK->GetPhysicalDeviceProperties2KHR(physicalDevice, &properties2);
    const auto& props = properties2.properties;
    dprintf(
      "Found GPU {:04x}:{:04x} with type {}: \"{}\"",
      props.vendorID,
      props.deviceID,
      static_cast<std::underlying_type_t<VkPhysicalDeviceType>>(
        props.deviceType),
      props.deviceName);
    if (id.deviceLUIDValid) {
      static_assert(VK_LUID_SIZE == sizeof(uint64_t));
      auto deviceLuid = std::bit_cast<uint64_t>(id.deviceLUID);
      dprintf("- Device LUID: {:#018x}", deviceLuid);
      if (deviceLuid == luid) {
        dprint("- Matching LUID, selecting device");
        mVKPhysicalDevice = physicalDevice;
      }
    }
  }

  if (!mVKPhysicalDevice) {
    OPENKNEEBOARD_LOG_AND_FATAL("Failed to find matching device");
  }

  uint32_t queueFamilyCount {0};
  mVK->GetPhysicalDeviceQueueFamilyProperties(
    mVKPhysicalDevice, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies;
  queueFamilies.resize(queueFamilyCount);
  mVK->GetPhysicalDeviceQueueFamilyProperties(
    mVKPhysicalDevice, &queueFamilyCount, queueFamilies.data());
  {
    auto it = std::ranges::find_if(
      queueFamilies, [](const VkQueueFamilyProperties& it) {
        return (it.queueFlags & VK_QUEUE_GRAPHICS_BIT);
      });
    if (it == queueFamilies.end()) {
      OPENKNEEBOARD_LOG_AND_FATAL("No graphics queues found");
    }
    mQueueFamilyIndex = (it - queueFamilies.begin());
  }

  float queuePriorities[] {1.0f};
  VkDeviceQueueCreateInfo queueCreateInfo {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = mQueueFamilyIndex,
    .queueCount = std::size(queuePriorities),
    .pQueuePriorities = queuePriorities,
  };

  const Vulkan::CombinedCreateInfo<
    SHM::Vulkan::DeviceCreateInfo,
    Vulkan::SpriteBatch::DeviceCreateInfo>
    deviceCreateInfo(VkDeviceCreateInfo {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueCreateInfo,
    });

  mDevice
    = mVK->make_unique_device(mVKPhysicalDevice, &deviceCreateInfo, nullptr);

  mVK->GetDeviceQueue(mDevice.get(), mQueueFamilyIndex, 0, &mQueue);

  mSpriteBatch = std::make_unique<Vulkan::SpriteBatch>(
    mVK.get(), mVKPhysicalDevice, mDevice.get(), nullptr, mQueueFamilyIndex, 0);

  VkCommandPoolCreateInfo poolCreateInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
      | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = mQueueFamilyIndex,
  };
  mCommandPool
    = mVK->make_unique<VkCommandPool>(mDevice.get(), &poolCreateInfo, nullptr);

  VkCommandBufferAllocateInfo commandAllocInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = mCommandPool.get(),
    .commandBufferCount = 1,
  };
  check_vkresult(mVK->AllocateCommandBuffers(
    mDevice.get(), &commandAllocInfo, &mCommandBuffer));

  VkFenceCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  mCompletionFence
    = mVK->make_unique<VkFence>(mDevice.get(), &createInfo, nullptr);
}

VulkanRenderer::~VulkanRenderer() {
  if (!mCompletionFence) {
    return;
  }

  VkFence fences[] {mCompletionFence.get()};
  check_vkresult(mVK->WaitForFences(
    mDevice.get(), std::size(fences), fences, true, ~(0ui64)));

  mVK->FreeCommandBuffers(
    mDevice.get(), mCommandPool.get(), 1, &mCommandBuffer);
}

SHM::CachedReader* VulkanRenderer::GetSHM() {
  return &mSHM;
}

std::wstring_view VulkanRenderer::GetName() const noexcept {
  return L"Vulkan";
}

void VulkanRenderer::Initialize(uint8_t swapchainLength) {
  if (mCompletionFence) {
    const auto fence = mCompletionFence.get();
    check_vkresult(
      mVK->WaitForFences(mDevice.get(), 1, &fence, true, ~(0ui64)));
  }
  mSHM.InitializeCache(
    mVK.get(),
    mVKInstance.get(),
    mDevice.get(),
    mVKPhysicalDevice,
    mQueueFamilyIndex,
    0,
    nullptr,
    swapchainLength);
}

void VulkanRenderer::SaveTextureToFile(
  SHM::IPCClientTexture* texture,
  const std::filesystem::path& path) {
  const auto source = reinterpret_cast<SHM::Vulkan::Texture*>(texture);

  this->SaveTextureToFile(
    source->GetDimensions(),
    source->GetVKImage(),
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    source->GetReadySemaphore(),
    source->GetReadySemaphoreValue(),
    path);
}

void VulkanRenderer::SaveTextureToFile(
  const PixelSize& dimensions,
  VkImage source,
  VkImageLayout sourceLayout,
  VkSemaphore waitSemaphore,
  uint64_t waitSemaphoreValue,
  const std::filesystem::path& path) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(activity, "SaveTextureToFile()");
  VkFence fences[] {mCompletionFence.get()};
  {
    OPENKNEEBOARD_TraceLoggingScope("FenceIn");
    mVK->WaitForFences(mDevice.get(), std::size(fences), fences, 1, ~(0ui64));
    mVK->ResetFences(mDevice.get(), std::size(fences), fences);
  }

  const auto dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
  const VkImageCreateInfo imageCreateInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_B8G8R8A8_UNORM,
    .extent = {dimensions.mWidth, dimensions.mHeight, 1},
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_LINEAR,
    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = &mQueueFamilyIndex,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  auto dest
    = mVK->make_unique<VkImage>(mDevice.get(), &imageCreateInfo, nullptr);
  VkMemoryRequirements memoryRequirements {};
  mVK->GetImageMemoryRequirements(
    mDevice.get(), dest.get(), &memoryRequirements);

  const auto memoryType = OpenKneeboard::Vulkan::FindMemoryType(
    mVK.get(),
    mVKPhysicalDevice,
    memoryRequirements.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  if (!memoryType) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Unable to find suitable memoryType");
  }

  const VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    .image = dest.get(),
  };

  const VkMemoryAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &dedicatedAllocInfo,
    .allocationSize = memoryRequirements.size,
    .memoryTypeIndex = *memoryType,
  };
  auto destMemory
    = mVK->make_unique<VkDeviceMemory>(mDevice.get(), &allocInfo, nullptr);

  const VkBindImageMemoryInfoKHR bindInfo {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
    .image = dest.get(),
    .memory = destMemory.get(),
  };

  check_vkresult(mVK->BindImageMemory2KHR(mDevice.get(), 1, &bindInfo));

  TraceLoggingWriteTagged(activity, "BeginCommandBuffer");

  auto cb = mCommandBuffer;

  const VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  check_vkresult(mVK->BeginCommandBuffer(cb, &beginInfo));

  VkImageMemoryBarrier inBarriers[] {
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = sourceLayout,
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
      .image = dest.get(),
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    },
  };

  mVK->CmdPipelineBarrier(
    cb,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    std::size(inBarriers),
    inBarriers);

  const VkImageCopy region {
    .srcSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .layerCount = 1,
    },
    .dstSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .layerCount = 1,
    },
    .extent = { dimensions.mWidth, dimensions.mHeight, 1 },
  };

  mVK->CmdCopyImage(
    cb,
    source,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    dest.get(),
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region);

  VkImageMemoryBarrier outBarriers[] {
    VkImageMemoryBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .newLayout = sourceLayout,
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
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .image = dest.get(),
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    },
  };

  mVK->CmdPipelineBarrier(
    cb,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    std::size(outBarriers),
    outBarriers);

  check_vkresult(mVK->EndCommandBuffer(cb));

  TraceLoggingWriteTagged(activity, "EndCommandBuffer");

  const VkTimelineSemaphoreSubmitInfoKHR semaphoreInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
    .waitSemaphoreValueCount = 1,
    .pWaitSemaphoreValues = &waitSemaphoreValue,
  };

  const VkPipelineStageFlags semaphoreStages {VK_PIPELINE_STAGE_TRANSFER_BIT};

  const VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &semaphoreInfo,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &waitSemaphore,
    .pWaitDstStageMask = &semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &cb,
  };

  check_vkresult(mVK->QueueSubmit(mQueue, 1, &submitInfo, fences[0]));
  {
    OPENKNEEBOARD_TraceLoggingScope("FenceOut");
    check_vkresult(mVK->WaitForFences(
      mDevice.get(), std::size(fences), fences, true, ~(0ui64)));
  }

  OPENKNEEBOARD_TraceLoggingScope("Export");
  auto mapping = mVK->MemoryMapping<uint8_t>(
    mDevice.get(), destMemory.get(), 0, VK_WHOLE_SIZE, 0);

  {
    OPENKNEEBOARD_TraceLoggingScope("InvalidateMapping");
    VkMappedMemoryRange range {
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
      .memory = destMemory.get(),
      .size = VK_WHOLE_SIZE,
    };
    check_vkresult(mVK->InvalidateMappedMemoryRanges(mDevice.get(), 1, &range));
  }

  // We're not actually using the GPU or D3D for this at all; this all
  // CPU and system RAM operations/resources.
  DXCPUImage cpuImage {
    .width = dimensions.mWidth,
    .height = dimensions.mHeight,
    .format = dxgiFormat,
    .pixels = mapping.get(),
  };
  winrt::check_hresult(DirectX::ComputePitch(
    dxgiFormat,
    dimensions.mWidth,
    dimensions.mHeight,
    cpuImage.rowPitch,
    cpuImage.slicePitch));
  winrt::check_hresult(DirectX::SaveToDDSFile(
    cpuImage, DirectX::DDS_FLAGS_NONE, path.wstring().c_str()));
}

uint64_t VulkanRenderer::Render(
  SHM::IPCClientTexture* sourceTexture,
  const PixelRect& sourceRect,
  HANDLE destTexture,
  const PixelSize& destTextureDimensions,
  const PixelRect& destRect,
  HANDLE semaphoreHandle,
  uint64_t semaphoreValueIn) {
  this->InitializeSemaphore(semaphoreHandle);
  this->InitializeDest(destTexture, destTextureDimensions);

  RenderDoc::NestedFrameCapture renderDocFrame(
    mVKInstance.get(), "VulkanRenderer::Render()");

  VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  check_vkresult(mVK->BeginCommandBuffer(mCommandBuffer, &beginInfo));

  VkImageMemoryBarrier inBarrier {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .image = mDestImage.get(),
    .subresourceRange = VkImageSubresourceRange {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1,
    },
  };

  mVK->CmdPipelineBarrier(
    mCommandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    1,
    &inBarrier);

  auto source = reinterpret_cast<SHM::Vulkan::Texture*>(sourceTexture);

  mSpriteBatch->Begin(
    mCommandBuffer, mDestImageView.get(), destTextureDimensions);

  mSpriteBatch->Draw(
    source->GetVKImageView(), source->GetDimensions(), sourceRect, destRect);

  mSpriteBatch->End();

  const VkImageMemoryBarrier outBarrier {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .image = mDestImage.get(),
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    };
  mVK->CmdPipelineBarrier(
    mCommandBuffer,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    0,
    0,
    nullptr,
    0,
    nullptr,
    1,
    &outBarrier);

  check_vkresult(mVK->EndCommandBuffer(mCommandBuffer));

  const auto semaphoreValueOut = semaphoreValueIn + 1;

  VkSemaphore waitSemaphores[] = {
    mSemaphore.get(),
    source->GetReadySemaphore(),
  };
  uint64_t waitSemaphoreValues[] = {
    semaphoreValueIn,
    source->GetReadySemaphoreValue(),
  };

  VkSemaphore signalSemaphores[] = {
    mSemaphore.get(),
  };
  uint64_t signalSemaphoreValues[] = {
    semaphoreValueOut,
  };

  VkTimelineSemaphoreSubmitInfoKHR semaphoreInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
    .waitSemaphoreValueCount = std::size(waitSemaphoreValues),
    .pWaitSemaphoreValues = waitSemaphoreValues,
    .signalSemaphoreValueCount = std::size(signalSemaphoreValues),
    .pSignalSemaphoreValues = signalSemaphoreValues,
  };

  VkPipelineStageFlags semaphoreStages[] {
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
  };

  VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &semaphoreInfo,
    .waitSemaphoreCount = std::size(waitSemaphores),
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &mCommandBuffer,
    .signalSemaphoreCount = std::size(signalSemaphores),
    .pSignalSemaphores = signalSemaphores,
  };

  VkFence fences[] {mCompletionFence.get()};
  check_vkresult(mVK->ResetFences(mDevice.get(), std::size(fences), fences));
  check_vkresult(mVK->QueueSubmit(mQueue, 1, &submitInfo, fences[0]));

  return semaphoreValueOut;
}

void VulkanRenderer::InitializeDest(
  HANDLE handle,
  const PixelSize& dimensions) {
  if (dimensions != mDestImageDimensions) {
    mDestHandle = {};
  }
  if (handle == mDestHandle) {
    return;
  }

  VkExternalMemoryImageCreateInfoKHR externalCreateInfo {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
  };

  // TODO: Using VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT here because - like all
  // the other renderers - I'm using an SRGB view on a UNORM texture. That
  // gets good results, but probably means I'm messing something up earlier.
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
    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = &mQueueFamilyIndex,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  mDestImage = mVK->make_unique<VkImage>(mDevice.get(), &createInfo, nullptr);

  VkImageMemoryRequirementsInfo2KHR memoryInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
    .image = mDestImage.get(),
  };

  VkMemoryRequirements2KHR memoryRequirements {
    .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
  };

  mVK->GetImageMemoryRequirements2KHR(
    mDevice.get(), &memoryInfo, &memoryRequirements);

  VkMemoryWin32HandlePropertiesKHR handleProperties {
    .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
  };

  check_vkresult(mVK->GetMemoryWin32HandlePropertiesKHR(
    mDevice.get(),
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
    handle,
    &handleProperties));

  const auto memoryType = OpenKneeboard::Vulkan::FindMemoryType(
    mVK.get(),
    mVKPhysicalDevice,
    handleProperties.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (!memoryType) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Unable to find suitable memoryType");
  }

  VkImportMemoryWin32HandleInfoKHR importInfo {
    .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
    .handle = handle,
  };

  VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
    .pNext = &importInfo,
    .image = mDestImage.get(),
  };

  VkMemoryAllocateInfo allocInfo {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &dedicatedAllocInfo,
    .allocationSize = memoryRequirements.memoryRequirements.size,
    .memoryTypeIndex = *memoryType,
  };

  mDestImageMemory
    = mVK->make_unique<VkDeviceMemory>(mDevice.get(), &allocInfo, nullptr);

  VkBindImageMemoryInfoKHR bindInfo {
    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
    .image = mDestImage.get(),
    .memory = mDestImageMemory.get(),
  };

  check_vkresult(mVK->BindImageMemory2KHR(mDevice.get(), 1, &bindInfo));

  VkImageViewCreateInfo viewCreateInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = mDestImage.get(),
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .subresourceRange = VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    };

  mDestImageView
    = mVK->make_unique<VkImageView>(mDevice.get(), &viewCreateInfo, nullptr);

  mDestHandle = handle;
  mDestImageDimensions = dimensions;
}

void VulkanRenderer::InitializeSemaphore(HANDLE handle) {
  if (handle == mSemaphoreHandle) {
    return;
  }

  VkFence fences[] {mCompletionFence.get()};
  check_vkresult(mVK->WaitForFences(
    mDevice.get(), std::size(fences), fences, false, ~(0ui64)));

  VkSemaphoreTypeCreateInfoKHR typeCreateInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
  };

  VkSemaphoreCreateInfo createInfo {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &typeCreateInfo,
  };

  mSemaphore
    = mVK->make_unique<VkSemaphore>(mDevice.get(), &createInfo, nullptr);

  VkImportSemaphoreWin32HandleInfoKHR importInfo {
    .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
    .semaphore = mSemaphore.get(),
    .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT,
    .handle = handle,
  };

  check_vkresult(
    mVK->ImportSemaphoreWin32HandleKHR(mDevice.get(), &importInfo));
}

}// namespace OpenKneeboard::Viewer