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

#include <OpenKneeboard/dprint.h>

using OpenKneeboard::Vulkan::check_vkresult;

namespace OpenKneeboard::Viewer {

static constexpr const char* const RequiredInstanceExtensions[] {
  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
#ifdef DEBUG
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

static constexpr const char* const RequiredLayers[] {
#ifdef DEBUG
  "VK_LAYER_KHRONOS_validation",
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
    (messageSeverity
     & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT))) {
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

  const VkInstanceCreateInfo baseInstanceCreateInfo {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = next,
    .pApplicationInfo = &applicationInfo,
    .enabledLayerCount = std::size(RequiredLayers),
    .ppEnabledLayerNames = RequiredLayers,
    .enabledExtensionCount = std::size(RequiredInstanceExtensions),
    .ppEnabledExtensionNames = RequiredInstanceExtensions,
  };
  const auto instanceCreateInfo = SHM::Vulkan::InstanceCreateInfo {
    Vulkan::SpriteBatch::InstanceCreateInfo {baseInstanceCreateInfo},
  };

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

  dprintf("Looking for GPU with LUID {:#018x}", luid);
  uint32_t physicalDeviceCount = 0;
  check_vkresult(mVK->EnumeratePhysicalDevices(
    mVKInstance.get(), &physicalDeviceCount, nullptr));
  std::vector<VkPhysicalDevice> physicalDevices {
    physicalDeviceCount, VK_NULL_HANDLE};
  check_vkresult(mVK->EnumeratePhysicalDevices(
    mVKInstance.get(), &physicalDeviceCount, physicalDevices.data()));

  VkPhysicalDevice matchingDevice {VK_NULL_HANDLE};
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
        matchingDevice = physicalDevice;
      }
    }
  }

  if (!matchingDevice) {
    OPENKNEEBOARD_LOG_AND_FATAL("Failed to find matching device");
  }
}

VulkanRenderer::~VulkanRenderer() = default;

SHM::CachedReader* VulkanRenderer::GetSHM() {
  return &mSHM;
}

std::wstring_view VulkanRenderer::GetName() const noexcept {
  return L"Vulkan";
}

void VulkanRenderer::Initialize(uint8_t swapchainLength) {
  // TODO
}

void VulkanRenderer::SaveTextureToFile(
  SHM::IPCClientTexture*,
  const std::filesystem::path&) {
  // TODO
}

uint64_t VulkanRenderer::Render(
  SHM::IPCClientTexture* sourceTexture,
  const PixelRect& sourceRect,
  HANDLE destTexture,
  const PixelSize& destTextureDimensions,
  const PixelRect& destRect,
  HANDLE fence,
  uint64_t fenceValueIn) {
  return fenceValueIn;
}

}// namespace OpenKneeboard::Viewer