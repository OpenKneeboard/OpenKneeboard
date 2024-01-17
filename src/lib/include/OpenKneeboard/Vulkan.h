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
#define VK_USE_PLATFORM_WIN32_KHR

#include <OpenKneeboard/dprint.h>

#include <vulkan/vulkan.h>

#include <stdexcept>

namespace OpenKneeboard::Vulkan {

constexpr bool VK_SUCCEEDED(VkResult code) {
  return code >= 0;
}

constexpr bool VK_FAILED(VkResult code) {
  return !VK_SUCCEEDED(code);
}

inline VkResult check_vkresult(VkResult code) {
  if (VK_FAILED(code)) {
    const auto message = std::format(
      "Vulkan call failed with VkResult {}", static_cast<int>(code));
    dprint(message);
    OPENKNEEBOARD_BREAK;
    throw std::runtime_error(message);
  }
  return code;
}

#define OPENKNEEBOARD_VK_FUNCS \
  IT(GetPhysicalDeviceProperties2) \
  IT(CreateCommandPool) \
  IT(DestroyCommandPool) \
  IT(AllocateCommandBuffers) \
  IT(ResetCommandBuffer) \
  IT(BeginCommandBuffer) \
  IT(EndCommandBuffer) \
  IT(CmdCopyImage) \
  IT(CmdBlitImage) \
  IT(CmdPipelineBarrier) \
  IT(CreateImage) \
  IT(DestroyImage) \
  IT(GetImageMemoryRequirements2) \
  IT(GetPhysicalDeviceMemoryProperties) \
  IT(GetMemoryWin32HandlePropertiesKHR) \
  IT(BindImageMemory2) \
  IT(AllocateMemory) \
  IT(CreateFence) \
  IT(DestroyFence) \
  IT(ResetFences) \
  IT(WaitForFences) \
  IT(QueueWaitIdle) \
  IT(CreateSemaphore) \
  IT(DestroySemaphore) \
  IT(ImportSemaphoreWin32HandleKHR) \
  IT(GetSemaphoreWin32HandleKHR) \
  IT(QueueSubmit) \
  IT(GetDeviceQueue)

#define OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES \
  IT(CommandPool) \
  IT(Image) \
  IT(Fence) \
  IT(Semaphore)

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
OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES
#undef IT

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

#define IT(resource) \
  using unique_Vk##resource \
    = std::unique_ptr<Vk##resource, Detail::Deleter<Vk##resource>>;
OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES
#undef IT

class Dispatch final {
 public:
  Dispatch() = delete;
  Dispatch(VkInstance, PFN_vkGetInstanceProcAddr);

#define IT(func) PFN_vk##func func {nullptr};
  OPENKNEEBOARD_VK_FUNCS
#undef IT

 private:
  template <class TResource>
  static auto make_unique_base(
    Detail::CreateFun<TResource> createImpl,
    Detail::DestroyFun<TResource> destroyImpl,
    VkDevice device,
    const typename Detail::CreateInfo<TResource>* createInfo,
    const VkAllocationCallbacks* allocator) {
    TResource ret {nullptr};
    check_vkresult(createImpl(device, createInfo, allocator, &ret));
    auto deleter = Detail::Deleter<TResource>(destroyImpl, device, allocator);
    return std::unique_ptr<TResource, decltype(deleter)>(ret, deleter);
  }

  template <class T>
  struct SmartPointerFactory;

#define IT(T) \
  template <> \
  struct SmartPointerFactory<Vk##T> { \
    static auto make_unique( \
      Dispatch* dispatch, \
      VkDevice device, \
      const typename Detail::CreateInfo<Vk##T>* createInfo, \
      const VkAllocationCallbacks* allocator) { \
      return make_unique_base<Vk##T>( \
        dispatch->Create##T, \
        dispatch->Destroy##T, \
        device, \
        createInfo, \
        allocator); \
    } \
  };
  OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES
#undef IT

 public:
  template <class T>
  auto make_unique(
    VkDevice device,
    const typename Detail::CreateInfo<T>* createInfo,
    const VkAllocationCallbacks* allocator) {
    return SmartPointerFactory<T>::make_unique(
      this, device, createInfo, allocator);
  }

  template <class T>
  auto make_unique(
    VkDevice device,
    const typename Detail::CreateInfo<T>& createInfo,
    const VkAllocationCallbacks* allocator) {
    return make_unique(device, &createInfo, allocator);
  }
};

}// namespace OpenKneeboard::Vulkan