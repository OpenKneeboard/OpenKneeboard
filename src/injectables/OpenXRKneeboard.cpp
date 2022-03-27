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

#define NEEDED_OPENXR_FUNCS \
  IT(xrCreateSession) \
  IT(xrEndFrame) \
  IT(xrCreateSwapchain)

static struct {
  PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr {nullptr};
#define IT(x) PFN_##x x {nullptr};
  NEEDED_OPENXR_FUNCS
#undef IT
} gOpenXR;

class OpenXRKneeboard {
 public:
  virtual ~OpenXRKneeboard();
  XrResult xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo,
    PFN_xrEndFrame next);

 private:
  XrSwapchain mSwapChain = nullptr;
};

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(ID3D11Device*);
  ~OpenXRD3D11Kneeboard();

 private:
  ID3D11Device* mDevice = nullptr;
};

OpenXRKneeboard::~OpenXRKneeboard() {
}

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(ID3D11Device* device)
  : mDevice(device) {
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
}

XrResult OpenXRKneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo,
  PFN_xrEndFrame next) {
  return next(session, frameEndInfo);
}

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
  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard = std::make_unique<OpenXRD3D11Kneeboard>(d3d11->device);
  }
  // TODO: D3D12

  return gOpenXR.xrCreateSession(instance, createInfo, session);
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
  if (gKneeboard) {
    return gKneeboard->xrEndFrame(session, frameEndInfo, gOpenXR.xrEndFrame);
  }
  return gOpenXR.xrEndFrame(session, frameEndInfo);
}

XrResult xrGetInstanceProcAddr(
  XrInstance instance,
  const char* name_cstr,
  PFN_xrVoidFunction* function) {
  std::string_view name {name_cstr};

  if (name == "xrCreateSession") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrCreateSession);
    return XR_SUCCESS;
  }
  if (name == "xrEndFrame") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrEndFrame);
    return XR_SUCCESS;
  }

  return gOpenXR.xrGetInstanceProcAddr(instance, name_cstr, function);
}

XrResult xrCreateApiLayerInstance(
  const XrInstanceCreateInfo* info,
  const struct XrApiLayerCreateInfo* layerInfo,
  XrInstance* instance) {
  // TODO: check version fields etc in layerInfo
  gOpenXR.xrGetInstanceProcAddr = layerInfo->nextInfo->nextGetInstanceProcAddr;

  XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
  nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
  auto nextResult = layerInfo->nextInfo->nextCreateApiLayerInstance(
    info, &nextLayerInfo, instance);
  if (nextResult != XR_SUCCESS) {
    return nextResult;
  }

#define IT(x) \
  if ( \
    gOpenXR.xrGetInstanceProcAddr( \
      *instance, #x, reinterpret_cast<PFN_xrVoidFunction*>(&gOpenXR.x)) \
    != XR_SUCCESS) { \
    return XR_ERROR_INITIALIZATION_FAILED; \
  }
  NEEDED_OPENXR_FUNCS
#undef IT

  return XR_SUCCESS;
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
