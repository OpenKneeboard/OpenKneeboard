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
#include <OpenKneeboard/config.h>
#include <d3d11.h>
#include <loader_interfaces.h>
#include <openxr/openxr.h>
#include <shims/winrt.h>

#include <memory>
#include <string>
#include <vector>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

const std::string_view OpenXRLayerName {"XR_APILAYER_NOVENDOR_OpenKneeboard"};

#define NEEDED_OPENXR_FUNCS \
  IT(xrCreateSession) \
  IT(xrEndFrame) \
  IT(xrCreateSwapchain) \
  IT(xrEnumerateSwapchainImages) \
  IT(xrDestroySwapchain)

static struct {
  PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr {nullptr};
#define IT(x) PFN_##x x {nullptr};
  NEEDED_OPENXR_FUNCS
#undef IT
} gOpenXR;

class OpenXRKneeboard {
 public:
  virtual ~OpenXRKneeboard();
  virtual XrResult xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo)
    = 0;

 private:
  XrSwapchain mSwapChain = nullptr;
};

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(XrSession, ID3D11Device*);
  ~OpenXRD3D11Kneeboard();

  virtual XrResult xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo) override;

 private:
  ID3D11Device* mDevice = nullptr;
  XrSwapchain mSwapchain = nullptr;
  std::vector<winrt::com_ptr<ID3D11Texture2D>> mTextures;
};

OpenXRKneeboard::~OpenXRKneeboard() {
}

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrSession session,
  ID3D11Device* device)
  : mDevice(device) {
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
    .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    .sampleCount = 1,
    .width = TextureWidth,
    .height = TextureHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };
  if (
    gOpenXR.xrCreateSwapchain(session, &swapchainInfo, &mSwapchain)
    != XR_SUCCESS) {
    mSwapchain = nullptr;
    return;
  }

  uint32_t imageCount = 0;
  gOpenXR.xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr);
  if (imageCount == 0) {
    return;
  }

  std::vector<XrSwapchainImageD3D11KHR> images(imageCount);
  if (
    gOpenXR.xrEnumerateSwapchainImages(
      mSwapchain,
      imageCount,
      &imageCount,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()))
    != XR_SUCCESS) {
    gOpenXR.xrDestroySwapchain(mSwapchain);
    mSwapchain = nullptr;
    return;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
    OPENKNEEBOARD_BREAK;
    gOpenXR.xrDestroySwapchain(mSwapchain);
    mSwapchain = nullptr;
    return;
  }

  for (auto i = 0; i < imageCount; ++i) {
#ifdef DEBUG
    if (images.at(i).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
      OPENKNEEBOARD_BREAK;
    }
#endif
    mTextures.at(i).copy_from(images.at(i).texture);
  }
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
}

XrResult OpenXRD3D11Kneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) {
  if (!mSwapchain) {
    return gOpenXR.xrEndFrame(session, frameEndInfo);
  }

  return gOpenXR.xrEndFrame(session, frameEndInfo);
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

  auto nextResult = gOpenXR.xrCreateSession(instance, createInfo, session);
  if (nextResult != XR_SUCCESS) {
    return nextResult;
  }

  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard
      = std::make_unique<OpenXRD3D11Kneeboard>(*session, d3d11->device);
  }
  // TODO: D3D12

  return XR_SUCCESS;
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
  if (gKneeboard) {
    return gKneeboard->xrEndFrame(session, frameEndInfo);
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
