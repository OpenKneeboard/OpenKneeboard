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

#include <OpenKneeboard/Pixels.h>

#include <OpenKneeboard/dprint.h>

#include <vulkan/vulkan.h>

#include <concepts>

namespace OpenKneeboard::Vulkan {

constexpr bool VK_SUCCEEDED(VkResult code) {
  return code >= 0;
}

constexpr bool VK_FAILED(VkResult code) {
  return !VK_SUCCEEDED(code);
}

inline VkResult check_vkresult(
  VkResult code,
  const std::source_location& loc = std::source_location::current()) {
  if (VK_FAILED(code)) {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "Vulkan call failed: {}", static_cast<int>(code));
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
  IT(GetDeviceQueue) \
  IT(CreateShaderModule) \
  IT(DestroyShaderModule)

#define OPENKNEEBOARD_VK_SMART_POINTER_RESOURCES \
  IT(CommandPool) \
  IT(Image) \
  IT(Fence) \
  IT(Semaphore) \
  IT(ShaderModule)

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
    const VkAllocationCallbacks* allocator,
    const std::source_location& loc) {
    TResource ret {nullptr};
    check_vkresult(createImpl(device, createInfo, allocator, &ret), loc);
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
      const VkAllocationCallbacks* allocator, \
      const std::source_location& loc = std::source_location::current()) { \
      return make_unique_base<Vk##T>( \
        dispatch->Create##T, \
        dispatch->Destroy##T, \
        device, \
        createInfo, \
        allocator, \
        loc); \
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

class Color final : public std::array<float, 4> {};

namespace Colors {
constexpr Color White {1, 1, 1, 1};
constexpr Color Transparent {0, 0, 0, 0};
};// namespace Colors

class SpriteBatch {
 public:
  SpriteBatch() = delete;
  SpriteBatch(
    Dispatch* dispatch,
    VkDevice device,
    const VkAllocationCallbacks* allocator,
    uint32_t queueFamilyIndex);
  ~SpriteBatch();

  void Draw(
    VkImageView source,
    const PixelSize& sourceSize,
    const PixelRect& sourceRect,
    const PixelRect& destRect,
    const Color& color = Colors::White,
    const std::source_location& loc = std::source_location::current());

  void End(const std::source_location& loc = std::source_location::current());

 private:
  unique_VkShaderModule mPixelShader;
  unique_VkShaderModule mVertexShader;

  unique_VkCommandPool mCommandPool;
  VkCommandBuffer mCommandBuffer {nullptr};

  PixelSize mDestSize;

  struct Vertex {
    uint32_t mTextureIndex {~(0ui32)};
    Color mColor {};
    std::array<float, 2> mTexCoord;
    std::array<float, 2> mPosition;
  };

  struct Sprite {
    VkImageView mSource {nullptr};
    PixelSize mSourceSize;
    PixelRect mSourceRect;
    PixelRect mDestRect;
    Color mColor;
  };

  bool mBetweenBeginAndEnd {false};
  std::vector<Sprite> mSprites;
};

}// namespace OpenKneeboard::Vulkan