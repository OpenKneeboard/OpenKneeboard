/*
 * OpenKneeboard
 *
 * Copyright (C) 2022-present Fred Emmott <fred@fredemmott.com>
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

#include "OpenXRVulkanKneeboard.h"

#include <OpenKneeboard/dprint.h>
#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_Vulkan
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

OpenXRVulkanKneeboard::OpenXRVulkanKneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingVulkanKHR& binding)
  : OpenXRKneeboard(session, runtimeID, next) {
  dprintf("{}", __FUNCTION__);
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() = default;

bool OpenXRVulkanKneeboard::ConfigurationsAreCompatible(
  const VRRenderConfig& /*initial*/,
  const VRRenderConfig& /*current*/) const {
  return true;
}

winrt::com_ptr<ID3D11Device> OpenXRVulkanKneeboard::GetD3D11Device() const {
  return mD3D11Device;
}

XrSwapchain OpenXRVulkanKneeboard::CreateSwapChain(
  XrSession,
  const VRRenderConfig&,
  uint8_t layerIndex) {
  // XXX FIXME XXX
  return {};
}

bool OpenXRVulkanKneeboard::Render(
  XrSwapchain swapchain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters&) {
  // XXX FIXME XXX
  return false;
}

}// namespace OpenKneeboard
