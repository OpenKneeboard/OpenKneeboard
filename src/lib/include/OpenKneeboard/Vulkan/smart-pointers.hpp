// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "impl/OPENKNEEBOARD_VK_FUNCS.h"
#include "vkresult.hpp"

#include <shims/vulkan/vulkan.h>

#include <concepts>

namespace OpenKneeboard::Vulkan {

// `(base, suffix)`, eg vkFooEXT has a base of `Foo` and a suffix of `EXT`
#define OPENKNEEBOARD_VK_SMART_POINTER_DEVICE_RESOURCES_CREATE_DESTROY \
  IT(Buffer, ) \
  IT(CommandPool, ) \
  IT(DescriptorPool, ) \
  IT(DescriptorSetLayout, ) \
  IT(Image, ) \
  IT(ImageView, ) \
  IT(Fence, ) \
  IT(PipelineLayout, ) \
  IT(Sampler, ) \
  IT(Semaphore, ) \
  IT(ShaderModule, )

#define OPENKNEEBOARD_VK_SMART_POINTER_DEVICE_RESOURCES \
  OPENKNEEBOARD_VK_SMART_POINTER_DEVICE_RESOURCES_CREATE_DESTROY \
  IT(DeviceMemory, )

#define OPENKNEEBOARD_VK_SMART_POINTER_INSTANCE_RESOURCES \
  IT(DebugUtilsMessenger, EXT)

#define OPENKNEEBOARD_VK_SMART_POINTER_CREATE_DESTROY_RESOURCES \
  OPENKNEEBOARD_VK_SMART_POINTER_DEVICE_RESOURCES_CREATE_DESTROY \
  OPENKNEEBOARD_VK_SMART_POINTER_INSTANCE_RESOURCES

namespace Detail {
template <class T>
struct ResourceTraits;

}// namespace Detail

// Handles that are tied to a device
template <class T>
concept creatable_device_resource = std::invocable<
  typename Detail::ResourceTraits<T>::CreateFun,
  VkDevice,
  typename Detail::ResourceTraits<T>::CreateInfo*,
  const VkAllocationCallbacks*,
  T*>;

template <class T>
concept destroyable_device_resource = std::invocable<
  typename Detail::ResourceTraits<T>::DestroyFun,
  VkDevice,
  T,
  const VkAllocationCallbacks*>;

template <class T>
concept manageable_device_resource
  = creatable_device_resource<T> && destroyable_device_resource<T>;

// Handles that are tied to an instance, but not a device
template <class T>
concept creatable_instance_resource = std::invocable<
  typename Detail::ResourceTraits<T>::CreateFun,
  VkInstance,
  typename Detail::ResourceTraits<T>::CreateInfo*,
  const VkAllocationCallbacks*,
  T*>;

template <class T>
concept destroyable_instance_resource = std::invocable<
  typename Detail::ResourceTraits<T>::DestroyFun,
  VkInstance,
  T,
  const VkAllocationCallbacks*>;

template <class T>
concept manageable_instance_resource
  = creatable_instance_resource<T> && destroyable_instance_resource<T>;

namespace Detail {

template <destroyable_device_resource T>
class DeviceResourceDeleter {
 public:
  using pointer = T;
  using DestroyFun = typename ResourceTraits<T>::DestroyFun;
  DeviceResourceDeleter() = default;
  constexpr DeviceResourceDeleter(
    DestroyFun impl,
    VkDevice device,
    const VkAllocationCallbacks* allocator) noexcept
    : mImpl(impl), mDevice(device), mAllocator(allocator) {
  }

  void operator()(T resource) const {
    std::invoke(mImpl, mDevice, resource, mAllocator);
  }

 private:
  DestroyFun mImpl {nullptr};
  VkDevice mDevice {nullptr};
  const VkAllocationCallbacks* mAllocator {nullptr};
};

#define IT(BASE, SUFFIX) \
  template <> \
  struct ResourceTraits<Vk##BASE##SUFFIX> { \
    using CreateFun = PFN_vkCreate##BASE##SUFFIX; \
    using CreateInfo = Vk##BASE##CreateInfo##SUFFIX; \
    using DestroyFun = PFN_vkDestroy##BASE##SUFFIX; \
    using Deleter = DeviceResourceDeleter<Vk##BASE##SUFFIX>; \
  };
OPENKNEEBOARD_VK_SMART_POINTER_DEVICE_RESOURCES_CREATE_DESTROY
#undef IT

template <>
struct ResourceTraits<VkDeviceMemory> {
  using CreateFun = PFN_vkAllocateMemory;
  using CreateInfo = VkMemoryAllocateInfo;
  using DestroyFun = PFN_vkFreeMemory;
  using Deleter = DeviceResourceDeleter<VkDeviceMemory>;
};

template <>
struct ResourceTraits<VkPipeline> {
  // No CreateFun or CreateInfo as they vary between compute and graphics
  // pipelines, and take different parameters to normal.
  using DestroyFun = PFN_vkDestroyPipeline;
  using Deleter = DeviceResourceDeleter<VkPipeline>;
};

template <class T>
class StandaloneDeleter {
 public:
  using pointer = T;
  using DestroyFun = void(VKAPI_PTR*)(T, const VkAllocationCallbacks*);

  StandaloneDeleter() = default;
  constexpr StandaloneDeleter(
    DestroyFun impl,
    const VkAllocationCallbacks* allocator)
    : mImpl(impl), mAllocator(allocator) {
  }

  inline void operator()(T instance) const {
    mImpl(instance, mAllocator);
  }

 private:
  DestroyFun mImpl {nullptr};
  const VkAllocationCallbacks* mAllocator {nullptr};
};

template <>
struct ResourceTraits<VkInstance> {
  using Deleter = StandaloneDeleter<VkInstance>;
};

template <>
struct ResourceTraits<VkDevice> {
  using Deleter = StandaloneDeleter<VkDevice>;
};

template <destroyable_instance_resource T>
class InstanceResourceDeleter {
 public:
  using pointer = T;
  using DestroyFun
    = void(VKAPI_PTR*)(VkInstance, T, const VkAllocationCallbacks*);

  InstanceResourceDeleter() = default;
  constexpr InstanceResourceDeleter(
    DestroyFun impl,
    VkInstance instance,
    const VkAllocationCallbacks* allocator)
    : mImpl(impl), mInstance(instance), mAllocator(allocator) {
  }

  inline void operator()(T resource) const {
    mImpl(mInstance, resource, mAllocator);
  }

 private:
  DestroyFun mImpl {nullptr};
  VkInstance mInstance {VK_NULL_HANDLE};
  const VkAllocationCallbacks* mAllocator {nullptr};
};

#define IT(BASE, SUFFIX) \
  template <> \
  struct ResourceTraits<Vk##BASE##SUFFIX> { \
    using CreateFun = PFN_vkCreate##BASE##SUFFIX; \
    using CreateInfo = Vk##BASE##CreateInfo##SUFFIX; \
    using DestroyFun = PFN_vkDestroy##BASE##SUFFIX; \
    using Deleter = InstanceResourceDeleter<Vk##BASE##SUFFIX>; \
  };
OPENKNEEBOARD_VK_SMART_POINTER_INSTANCE_RESOURCES
#undef IT

}// namespace Detail

// Just some basic tests

// vkCreateFoo and vkDestroyFoo
static_assert(manageable_device_resource<VkBuffer>);

// vkAllocateMemory and vkFreeMemory
static_assert(manageable_device_resource<VkDeviceMemory>);

// vkDestroyFoo, but no matching vkCreateFoo
static_assert(destroyable_device_resource<VkPipeline>);
static_assert(!creatable_device_resource<VkPipeline>);
static_assert(!manageable_device_resource<VkPipeline>);

static_assert(manageable_instance_resource<VkDebugUtilsMessengerEXT>);
static_assert(!manageable_device_resource<VkDebugUtilsMessengerEXT>);

template <class T>
using unique_vk
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
  MemoryMapping& operator=(MemoryMapping&& other) noexcept {
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