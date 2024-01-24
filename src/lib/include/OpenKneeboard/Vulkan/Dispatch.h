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
#pragma once

#include <OpenKneeboard/Vulkan/impl/OPENKNEEBOARD_VK_FUNCS.h>
#include <OpenKneeboard/Vulkan/smart-pointers.h>
#include <OpenKneeboard/Vulkan/vkresult.h>

namespace OpenKneeboard::Vulkan {

class Dispatch final {
 public:
  Dispatch() = delete;
  Dispatch(VkInstance, PFN_vkGetInstanceProcAddr);

#define IT(func) PFN_vk##func func {nullptr};
  OPENKNEEBOARD_VK_FUNCS
#undef IT

 private:
  template <creatable_device_handle T>
  auto GetCreateFun() const;
  template <destroyable_device_handle T>
  auto GetDestroyFun() const;

#define IT(T) \
  template <> \
  auto GetCreateFun<Vk##T>() const { \
    return this->Create##T; \
  } \
  template <> \
  auto GetDestroyFun<Vk##T>() const { \
    return this->Destroy##T; \
  }
  OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES_CREATE_DESTROY
#undef IT
  template <>
  auto GetCreateFun<VkDeviceMemory>() const {
    return this->AllocateMemory;
  }
  template <>
  auto GetDestroyFun<VkDeviceMemory>() const {
    return this->FreeMemory;
  }

 public:
  template <manageable_device_handle TResource>
  auto make_unique(
    VkDevice device,
    const typename Detail::CreateInfo<TResource>* createInfo,
    const VkAllocationCallbacks* allocator,
    const std::source_location& loc = std::source_location::current()) {
    auto createImpl = this->GetCreateFun<TResource>();
    auto destroyImpl = this->GetDestroyFun<TResource>();
    TResource ret {VK_NULL_HANDLE};
    check_vkresult(createImpl(device, createInfo, allocator, &ret), loc);
    return unique_vk<TResource>(ret, {destroyImpl, device, allocator});
  }

  inline auto make_unique_device(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator) {
    VkDevice ret {VK_NULL_HANDLE};
    check_vkresult(
      this->CreateDevice(physicalDevice, createInfo, allocator, &ret));
    return unique_vk<VkDevice>(ret, {this->DestroyDevice, allocator});
  }

  inline auto make_unique_graphics_pipeline(
    VkDevice device,
    VkPipelineCache pipelineCache,
    const VkGraphicsPipelineCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator) {
    VkPipeline ret {VK_NULL_HANDLE};
    check_vkresult(this->CreateGraphicsPipelines(
      device, pipelineCache, 1, createInfo, allocator, &ret));
    return unique_vk<VkPipeline>(
      ret, {this->DestroyPipeline, device, allocator});
  }

  template <class T>
  auto MemoryMapping(
    VkDevice device,
    VkDeviceMemory deviceMemory,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkMemoryMapFlags flags) {
    return Vulkan::MemoryMapping<T> {
      this->MapMemory,
      this->UnmapMemory,
      device,
      deviceMemory,
      offset,
      size,
      flags,
    };
  }
};
}// namespace OpenKneeboard::Vulkan