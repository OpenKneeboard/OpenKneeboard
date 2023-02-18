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
#include <OpenKneeboard/scope_guard.h>
#include <dxgi1_6.h>
#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

static inline constexpr bool VK_SUCCEEDED(VkResult code) {
  return code >= 0;
}

static inline constexpr bool VK_FAILED(VkResult code) {
  return !VK_SUCCEEDED(code);
}

namespace OpenKneeboard {

OpenXRVulkanKneeboard::OpenXRVulkanKneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingVulkanKHR& binding,
  PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr)
  : OpenXRKneeboard(session, runtimeID, next) {
  dprintf("{}", __FUNCTION__);

#define IT(vkfun) \
  mPFN_##vkfun = reinterpret_cast<PFN_##vkfun>( \
    pfnVkGetInstanceProcAddr(binding.instance, #vkfun));
  OPENKNEEBOARD_VK_FUNCS
#undef IT

  this->InitializeD3D11(binding.physicalDevice);
}

void OpenXRVulkanKneeboard::InitializeD3D11(VkPhysicalDevice physicalDevice) {
  VkPhysicalDeviceIDProperties deviceIDProps {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};

  VkPhysicalDeviceProperties2 deviceProps2 {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  deviceProps2.pNext = &deviceIDProps;
  mPFN_vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);

  if (deviceIDProps.deviceLUIDValid == VK_FALSE) {
    dprint("Failed to get Vulkan device LUID");
    return;
  }

  UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  auto d3dLevel = D3D_FEATURE_LEVEL_11_0;
  UINT dxgiFlags = 0;

#ifdef DEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
  dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  winrt::com_ptr<IDXGIFactory> dxgiFactory;
  winrt::check_hresult(
    CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(dxgiFactory.put())));

  winrt::com_ptr<IDXGIAdapter> adapterIt;
  winrt::com_ptr<IDXGIAdapter> adapter;
  for (unsigned int i = 0; dxgiFactory->EnumAdapters(i, adapterIt.put()); ++i) {
    if (!adapterIt) {
      break;
    }
    const scope_guard releaseAdapter([&]() { adapterIt = {nullptr}; });

    DXGI_ADAPTER_DESC desc;
    winrt::check_hresult(adapterIt->GetDesc(&desc));

    if (
      memcmp(&desc.AdapterLuid, deviceIDProps.deviceLUID, sizeof(LUID)) != 0) {
      continue;
    }

    dprintf(
      L"Found DXGI adapter with matching LUID for VkPhysicalDevice: {}",
      desc.Description);
    adapter = adapterIt;
    break;
  }
  if (!adapter) {
    dprint("Couldn't find DXGI adapter with LUID matching VkPhysicalDevice");
    return;
  }

  winrt::check_hresult(D3D11CreateDevice(
    adapter.get(),
    // UNKNOWN is reuqired when specifying an adapter
    D3D_DRIVER_TYPE_UNKNOWN,
    nullptr,
    d3dFlags,
    &d3dLevel,
    1,
    D3D11_SDK_VERSION,
    mD3D11Device.put(),
    nullptr,
    nullptr));
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
