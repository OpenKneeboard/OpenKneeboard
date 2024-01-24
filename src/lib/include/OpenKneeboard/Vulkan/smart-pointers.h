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
  IT(ImageView) \
  IT(Fence) \
  IT(PipelineLayout) \
  IT(Sampler) \
  IT(Semaphore) \
  IT(ShaderModule)

#define OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES \
  OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES_CREATE_DESTROY \
  IT(DeviceMemory)

namespace Detail {
template <class T>
struct ResourceTraits;

template <class T>
using CreateFun = ResourceTraits<T>::CreateFun;
template <class T>
using CreateInfo = ResourceTraits<T>::CreateInfo;
template <class T>
using DestroyFun = ResourceTraits<T>::DestroyFun;

// clang-format off
template<class TFun, class TResource>
concept create_fun = std::invocable<
  TFun,
  VkDevice,
  Detail::CreateInfo<TResource>*,
  const VkAllocationCallbacks*,
  TResource*
>;

template <class TFun, class TResource>
concept destroy_fun = std::invocable<
  TFun,
  VkDevice,
  TResource,
  const VkAllocationCallbacks*
>;
// clang-format on

}// namespace Detail

template <class T>
concept creatable_handle = Detail::create_fun<Detail::CreateFun<T>, T>;

template <class T>
concept destroyable_handle = Detail::destroy_fun<Detail::DestroyFun<T>, T>;

template <class T>
concept manageable_handle = creatable_handle<T> && destroyable_handle<T>;

namespace Detail {

template <destroyable_handle T>
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

class InstanceDeleter {
 public:
  using pointer = VkInstance;
  InstanceDeleter() = default;
  constexpr InstanceDeleter(
    PFN_vkDestroyInstance impl,
    const VkAllocationCallbacks* allocator)
    : mImpl(impl), mAllocator(allocator) {
  }

  inline void operator()(VkInstance instance) const {
    mImpl(instance, mAllocator);
  }

 private:
  PFN_vkDestroyInstance mImpl {nullptr};
  const VkAllocationCallbacks* mAllocator {nullptr};
};

#define IT(T) \
  template <> \
  struct ResourceTraits<Vk##T> { \
    using CreateFun = PFN_vkCreate##T; \
    using CreateInfo = Vk##T##CreateInfo; \
    using DestroyFun = PFN_vkDestroy##T; \
    using Deleter = Deleter<Vk##T>; \
  };
OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES_CREATE_DESTROY
#undef IT

template <>
struct ResourceTraits<VkDeviceMemory> {
  using CreateFun = PFN_vkAllocateMemory;
  using CreateInfo = VkMemoryAllocateInfo;
  using DestroyFun = PFN_vkFreeMemory;
  using Deleter = Deleter<VkDeviceMemory>;
};

template <>
struct ResourceTraits<VkPipeline> {
  // No CreateFun or CreateInfo as they vary between compute and graphics
  // pipelines, and take different parameters to normal.
  using DestroyFun = PFN_vkDestroyPipeline;
  using Deleter = Deleter<VkPipeline>;
};

template <>
struct ResourceTraits<VkInstance> {
  using Deleter = InstanceDeleter;
};

}// namespace Detail

// Just some basic tests

// vkCreateFoo and vkDestroyFoo
static_assert(manageable_handle<VkBuffer>);

// vkAllocateMemory and vkFreeMemory
static_assert(manageable_handle<VkDeviceMemory>);

// vkDestroyFoo, but no matching vkCreateFoo
static_assert(destroyable_handle<VkPipeline>);
static_assert(!creatable_handle<VkPipeline>);
static_assert(!manageable_handle<VkPipeline>);

template <class T>
using unique_ptr
  = std::unique_ptr<T, typename Detail::ResourceTraits<T>::Deleter>;

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