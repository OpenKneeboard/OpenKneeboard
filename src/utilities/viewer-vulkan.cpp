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

#include <ztd/out_ptr.hpp>

namespace zop = ztd::out_ptr;

using OpenKneeboard::Vulkan::check_vkresult;

namespace OpenKneeboard::Viewer {

static constexpr const char* const RequiredInstanceExtensions[] {
  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
};

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

  const VkInstanceCreateInfo baseInstanceCreateInfo {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &applicationInfo,
    .enabledExtensionCount = std::size(RequiredInstanceExtensions),
    .ppEnabledExtensionNames = RequiredInstanceExtensions,
  };
  const auto instanceCreateInfo = SHM::Vulkan::InstanceCreateInfo {
    Vulkan::SpriteBatch::InstanceCreateInfo {baseInstanceCreateInfo},
  };

  check_vkresult(
    vkCreateInstance(&instanceCreateInfo, nullptr, zop::out_ptr(mVKInstance)));

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