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
  IT(ResetFences) \
  IT(WaitForFences) \
  IT(QueueWaitIdle) \
  IT(CreateSemaphore) \
  IT(ImportSemaphoreWin32HandleKHR) \
  IT(GetSemaphoreWin32HandleKHR) \
  IT(QueueSubmit) \
  IT(GetDeviceQueue)

class Dispatch final {
 public:
  Dispatch() = delete;
  Dispatch(VkInstance, PFN_vkGetInstanceProcAddr);

#define IT(func) PFN_vk##func func {nullptr};
  OPENKNEEBOARD_VK_FUNCS
#undef IT
};

}// namespace OpenKneeboard::Vulkan