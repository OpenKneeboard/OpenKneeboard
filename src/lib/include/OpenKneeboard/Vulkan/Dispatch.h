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
  template <class T>
  auto GetCreateFun() const;
  template <class T>
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
  template <manageable_handle TResource>
  auto make_unique(
    VkDevice device,
    const typename Detail::CreateInfo<TResource>* createInfo,
    const VkAllocationCallbacks* allocator,
    const std::source_location& loc = std::source_location::current()) {
    auto createImpl = this->GetCreateFun<TResource>();
    auto destroyImpl = this->GetDestroyFun<TResource>();
    TResource ret {nullptr};
    check_vkresult(createImpl(device, createInfo, allocator, &ret), loc);
    auto deleter = Detail::Deleter<TResource>(destroyImpl, device, allocator);
    return OpenKneeboard::Vulkan::unique_ptr<TResource>(ret, deleter);
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