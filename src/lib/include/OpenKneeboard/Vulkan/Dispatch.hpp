// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Vulkan/impl/OPENKNEEBOARD_VK_FUNCS.h>
#include <OpenKneeboard/Vulkan/smart-pointers.hpp>
#include <OpenKneeboard/Vulkan/vkresult.hpp>

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

#define IT(BASE, SUFFIX) \
  template <> \
  auto GetCreateFun<Vk##BASE##SUFFIX>() const { \
    return this->Create##BASE##SUFFIX; \
  } \
  template <> \
  auto GetDestroyFun<Vk##BASE##SUFFIX>() const { \
    return this->Destroy##BASE##SUFFIX; \
  }
  OPENKNEEBOARD_VK_SMART_POINTER_DEVICE_RESOURCES_CREATE_DESTROY
  OPENKNEEBOARD_VK_SMART_POINTER_INSTANCE_RESOURCES
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
  template <
    manageable_device_resource TResource,
    class Traits = typename Detail::ResourceTraits<TResource>>
  auto make_unique(
    VkDevice device,
    const typename Traits::CreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    const std::source_location& loc = std::source_location::current()) {
    auto createImpl = this->GetCreateFun<TResource>();
    auto destroyImpl = this->GetDestroyFun<TResource>();

    TResource ret {VK_NULL_HANDLE};
    check_vkresult(createImpl(device, createInfo, allocator, &ret), loc);
    return unique_vk<TResource>(ret, {destroyImpl, device, allocator});
  }

  template <
    manageable_instance_resource TResource,
    class Traits = typename Detail::ResourceTraits<TResource>>
  auto make_unique(
    VkInstance instance,
    const typename Traits::CreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    const std::source_location& loc = std::source_location::current()) {
    auto createImpl = this->GetCreateFun<TResource>();
    auto destroyImpl = this->GetDestroyFun<TResource>();

    TResource ret {VK_NULL_HANDLE};
    check_vkresult(createImpl(instance, createInfo, allocator, &ret), loc);
    return unique_vk<TResource>(ret, {destroyImpl, instance, allocator});
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
