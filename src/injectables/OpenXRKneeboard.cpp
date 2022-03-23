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
#include <d3d11.h>
#include <loader_interfaces.h>
#include <openxr/openxr.h>

#include <memory>
#include <string>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {
const std::string_view OpenXRLayerName {"XR_APILAYER_NOVENDOR_OpenKneeboard"};

static PFN_xrGetInstanceProcAddr gNextXrGetInstanceProcAddr = nullptr;

class OpenXRKneeboard {};
class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(ID3D11Device*) {};
};

static std::unique_ptr<OpenXRKneeboard> gKneeboard;

template <class T>
static const T* findInXrNextChain(XrStructureType type, const void* next) {
  while (next) {
    auto base = reinterpret_cast<const XrBaseInStructure*>(next);
    if (base->type == type) {
      return reinterpret_cast<const T*>(base);
    }
    next = base->next;
  }
  return nullptr;
}

XrResult xrCreateSession(
  XrInstance instance,
  const XrSessionCreateInfo* createInfo,
  XrSession* session) {
  static PFN_xrCreateSession sNext = nullptr;
  if (!sNext) {
    if (
      gNextXrGetInstanceProcAddr(
        instance,
        "xrCreateSession",
        reinterpret_cast<PFN_xrVoidFunction*>(&sNext))
      != XR_SUCCESS) {
      sNext = nullptr;
      return XR_ERROR_INITIALIZATION_FAILED;
    }
  }

  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard = std::make_unique<OpenXRD3D11Kneeboard>(d3d11->device);
  }
  // TODO: D3D12

  return sNext(instance, createInfo, session);
}

XrResult xrGetInstanceProcAddr(
  XrInstance instance,
  const char* name,
  PFN_xrVoidFunction* function) {
  if (name == std::string_view("xrCreateSession")) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrCreateSession);
    return XR_SUCCESS;
  }
  return gNextXrGetInstanceProcAddr(instance, name, function);
}

XrResult xrCreateApiLayerInstance(
  const XrInstanceCreateInfo* info,
  const struct XrApiLayerCreateInfo* layerInfo,
  XrInstance* instance) {
  // TODO: check version fields etc in layerInfo
  gNextXrGetInstanceProcAddr = layerInfo->nextInfo->nextGetInstanceProcAddr;

  XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
  nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
  return layerInfo->nextInfo->nextCreateApiLayerInstance(
    info, &nextLayerInfo, instance);
}

}// namespace OpenKneeboard

extern "C" {
XrResult __declspec(dllexport) XRAPI_CALL
  OpenKneeboard_xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* loaderInfo,
    const char* layerName,
    XrNegotiateApiLayerRequest* apiLayerRequest) {
  if (layerName != OpenKneeboard::OpenXRLayerName) {
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  // TODO: check version fields etc in loaderInfo

  apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
  apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
  apiLayerRequest->getInstanceProcAddr = &OpenKneeboard::xrGetInstanceProcAddr;
  apiLayerRequest->createApiLayerInstance
    = &OpenKneeboard::xrCreateApiLayerInstance;
  return XR_SUCCESS;
}
}
