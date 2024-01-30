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

// This file is mostly independent of graphics APIs, however we need to
// pull in vulkan here to get the definition of `xrCreateVulkanDeviceKHR()`
//
// ... but, as that means we need to pull in openxr_platform.h now and that
// can only be included once, we need to pull in all the supported APIs
#include <vulkan/vulkan.h>

#include <d3d11.h>
#include <d3d12.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <utility>

#define OPENKNEEBOARD_NEXT_OPENXR_FUNCS \
  IT(xrAcquireSwapchainImage) \
  IT(xrCreateReferenceSpace) \
  IT(xrCreateSession) \
  IT(xrCreateSwapchain) \
  IT(xrCreateVulkanDeviceKHR) \
  IT(xrCreateVulkanInstanceKHR) \
  IT(xrDestroyInstance) \
  IT(xrDestroySession) \
  IT(xrDestroySpace) \
  IT(xrDestroySwapchain) \
  IT(xrEndFrame) \
  IT(xrEnumerateSwapchainFormats) \
  IT(xrEnumerateSwapchainImages) \
  IT(xrGetInstanceProperties) \
  IT(xrGetSystemProperties) \
  IT(xrGetVulkanDeviceExtensionsKHR) \
  IT(xrGetVulkanGraphicsRequirementsKHR) \
  IT(xrGetVulkanInstanceExtensionsKHR) \
  IT(xrLocateSpace) \
  IT(xrReleaseSwapchainImage) \
  IT(xrWaitSwapchainImage)

namespace OpenKneeboard {

class OpenXRNext final {
 public:
  OpenXRNext(XrInstance, PFN_xrGetInstanceProcAddr);

#define IT(func) \
  template <class... Args> \
  auto func(Args&&... args) { \
    return this->m_##func(std::forward<Args>(args)...); \
  }
  IT(xrGetInstanceProcAddr)
  OPENKNEEBOARD_NEXT_OPENXR_FUNCS
#undef IT

 private:
#define IT(func) PFN_##func m_##func {nullptr};
  IT(xrGetInstanceProcAddr)
  OPENKNEEBOARD_NEXT_OPENXR_FUNCS
#undef IT
};

}// namespace OpenKneeboard
