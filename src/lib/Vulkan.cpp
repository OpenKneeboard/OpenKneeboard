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

#include <OpenKneeboard/Vulkan.h>

namespace OpenKneeboard::Vulkan {

Dispatch::Dispatch(
  VkInstance instance,
  PFN_vkGetInstanceProcAddr getInstanceProcAddr) {
#define IT(vkfun) \
  this->##vkfun = reinterpret_cast<PFN_vk##vkfun>( \
    getInstanceProcAddr(instance, "vk" #vkfun));
  OPENKNEEBOARD_VK_FUNCS
#undef IT
}

std::optional<uint32_t> FindMemoryType(
  Dispatch* vk,
  VkPhysicalDevice physicalDevice,
  uint32_t filter,
  VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties properties;
  vk->GetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if (!(filter & (1 << i))) {
      continue;
    }
    if ((flags & properties.memoryTypes[i].propertyFlags) == flags) {
      return i;
    }
  }

  return {};
}

}// namespace OpenKneeboard::Vulkan