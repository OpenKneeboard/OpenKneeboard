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
  IT(DescriptorPool) \
  IT(DescriptorSetLayout) \
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
struct ResourceTraits;

#define IT(T) \
  template <> \
  struct ResourceTraits<Vk##T> { \
    using CreateFun = PFN_vkCreate##T; \
    using CreateInfo = Vk##T##CreateInfo; \
    using DestroyFun = PFN_vkDestroy##T; \
  };
OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES_CREATE_DESTROY
#undef IT

template <>
struct ResourceTraits<VkDeviceMemory> {
  using CreateFun = PFN_vkAllocateMemory;
  using CreateInfo = VkMemoryAllocateInfo;
  using DestroyFun = PFN_vkFreeMemory;
};

template <class T>
using CreateFun = ResourceTraits<T>::CreateFun;
template <class T>
using CreateInfo = ResourceTraits<T>::CreateInfo;
template <class T>
using DestroyFun = ResourceTraits<T>::DestroyFun;

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

namespace Detail {

template <manageable_handle T>
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

template <manageable_handle T>
using unique_ptr = std::unique_ptr<T, Detail::Deleter<T>>;

template <class T>
class MemoryMapping {
 public:
  constexpr MemoryMapping() = default;
  constexpr MemoryMapping(nullptr_t) {
  }

  MemoryMapping(
    PFN_vkMapMemory mapMemory,
    PFN_vkUnmapMemory unmapMemory,
    VkDevice device,
    VkDeviceMemory deviceMemory,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkMemoryMapFlags flags)
    : mUnmapMemory(unmapMemory), mDevice(device), mDeviceMemory(deviceMemory) {
    check_vkresult(
      mapMemory(device, deviceMemory, offset, size, flags, &mData));
  }

  ~MemoryMapping() {
    if (mData) {
      mUnmapMemory(mDevice, mDeviceMemory);
    }
  }

  T* get() const {
    return reinterpret_cast<T*>(mData);
  }

  constexpr operator bool() const {
    return !!mData;
  }

  MemoryMapping(const MemoryMapping&) = delete;
  MemoryMapping(MemoryMapping&&) = delete;
  MemoryMapping& operator=(const MemoryMapping&) = delete;
  MemoryMapping& operator=(MemoryMapping&& other) {
    if (mData) {
      mUnmapMemory(mDevice, mDeviceMemory);
    }

    this->mUnmapMemory = other.mUnmapMemory;
    this->mDevice = other.mDevice;
    this->mDeviceMemory = other.mDeviceMemory;
    this->mData = other.mData;

    other.mData = nullptr;

    return *this;
  }

 private:
  PFN_vkUnmapMemory mUnmapMemory {nullptr};
  VkDevice mDevice {nullptr};
  VkDeviceMemory mDeviceMemory {nullptr};
  void* mData {nullptr};
};

}// namespace OpenKneeboard::Vulkan