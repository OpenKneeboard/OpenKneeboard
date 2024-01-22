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

#include "impl/OPENKNEEBOARD_VK_FUNCS.h"
#include "vkresult.h"

#include <vulkan/vulkan.h>

#include <concepts>

namespace OpenKneeboard::Vulkan {

#define OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES_CREATE_DESTROY \
  IT(Buffer) \
  IT(CommandPool) \
  IT(Image) \
  IT(Fence) \
  IT(Sampler) \
  IT(Semaphore) \
  IT(ShaderModule)

#define OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES \
  OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES_CREATE_DESTROY \
  IT(DeviceMemory)

namespace Detail {
template <class T>
struct ResourceInfo;

#define IT(T) \
  template <> \
  struct ResourceInfo<Vk##T> { \
    using CreateFun = PFN_vkCreate##T; \
    using CreateInfo = Vk##T##CreateInfo; \
    using DestroyFun = PFN_vkDestroy##T; \
  };
OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES_CREATE_DESTROY
#undef IT

template <>
struct ResourceInfo<VkDeviceMemory> {
  using CreateFun = PFN_vkAllocateMemory;
  using CreateInfo = VkMemoryAllocateInfo;
  using DestroyFun = PFN_vkFreeMemory;
};

template <class T>
using CreateFun = ResourceInfo<T>::CreateFun;
template <class T>
using CreateInfo = ResourceInfo<T>::CreateInfo;
template <class T>
using DestroyFun = ResourceInfo<T>::DestroyFun;

template <class T>
class Deleter {
 public:
  using pointer = T;
  Deleter() = default;
  constexpr Deleter(
    DestroyFun<T> impl,
    VkDevice device,
    const VkAllocationCallbacks* allocator) noexcept
    : mImpl(impl), mDevice(device), mAllocator(allocator) {
  }

  void operator()(T resource) const {
    std::invoke(mImpl, mDevice, resource, mAllocator);
  }

 private:
  DestroyFun<T> mImpl {nullptr};
  VkDevice mDevice {nullptr};
  const VkAllocationCallbacks* mAllocator {nullptr};
};

}// namespace Detail

// clang-format off
template <class T>
concept manageable_handle =
  std::invocable<
    Detail::CreateFun<T>,
    VkDevice,
    Detail::CreateInfo<T>*,
    const VkAllocationCallbacks*,
    T*
  > && std::invocable<
    Detail::DestroyFun<T>,
    VkDevice,
    T,
    const VkAllocationCallbacks*
  >;
// clang-format on

#define IT(resource) \
  static_assert(manageable_handle<Vk##resource>); \
  using unique_Vk##resource \
    = std::unique_ptr<Vk##resource, Detail::Deleter<Vk##resource>>;
OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES
#undef IT

}// namespace OpenKneeboard::Vulkan