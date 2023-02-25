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

#include "OpenXRKneeboard.h"

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>
#include <loader_interfaces.h>

#include <memory>
#include <string>

#include "OpenXRD3D11Kneeboard.h"
#include "OpenXRD3D12Kneeboard.h"
#include "OpenXRNext.h"
#include "OpenXRVulkanKneeboard.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

static constexpr XrPosef XR_POSEF_IDENTITY {
  .orientation = {0.0f, 0.0f, 0.0f, 1.0f},
  .position = {0.0f, 0.0f, 0.0f},
};

constexpr std::string_view OpenXRLayerName {
  "XR_APILAYER_FREDEMMOTT_OpenKneeboard"};
static_assert(OpenXRLayerName.size() <= XR_MAX_API_LAYER_NAME_SIZE);

// Don't use a unique_ptr as on process exit, windows doesn't clean these up
// in a useful order, and Microsoft recommend just leaking resources on
// thread/process exit
//
// In this case, it leads to an infinite hang on ^C
static OpenXRKneeboard* gKneeboard {nullptr};

static std::shared_ptr<OpenXRNext> gNext;
static OpenXRRuntimeID gRuntime {};
static PFN_vkGetInstanceProcAddr gPFN_vkGetInstanceProcAddr {nullptr};
static const VkAllocationCallbacks* gVKAllocator {nullptr};
static unique_hmodule gLibVulkan {LoadLibraryW(L"vulkan-1.dll")};

OpenXRKneeboard::OpenXRKneeboard(
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next)
  : mOpenXR(next) {
  dprintf("{}", __FUNCTION__);

  if (std::string_view(runtimeID.mName).starts_with("Varjo")) {
    dprint("Varjo runtime detected");
    mIsVarjoRuntime = true;
  }

  mSwapchains.fill(nullptr);
  mRenderCacheKeys.fill(~(0ui64));

  XrReferenceSpaceCreateInfo referenceSpace {
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .next = nullptr,
    .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
    .poseInReferenceSpace = XR_POSEF_IDENTITY,
  };

  auto oxr = next.get();

  XrResult nextResult
    = oxr->xrCreateReferenceSpace(session, &referenceSpace, &mLocalSpace);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to create LOCAL reference space: {}", nextResult);
    return;
  }

  referenceSpace.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  nextResult
    = oxr->xrCreateReferenceSpace(session, &referenceSpace, &mViewSpace);
  if (nextResult != XR_SUCCESS) {
    dprintf("Failed to create VIEW reference space: {}", nextResult);
    return;
  }
}

OpenXRKneeboard::~OpenXRKneeboard() {
  if (mLocalSpace) {
    mOpenXR->xrDestroySpace(mLocalSpace);
  }
  if (mViewSpace) {
    mOpenXR->xrDestroySpace(mViewSpace);
  }

  for (auto swapchain: mSwapchains) {
    if (swapchain) {
      mOpenXR->xrDestroySwapchain(swapchain);
    }
  }
}

bool OpenXRKneeboard::IsVarjoRuntime() const {
  return mIsVarjoRuntime;
}

OpenXRNext* OpenXRKneeboard::GetOpenXR() {
  return mOpenXR.get();
}

XrResult OpenXRKneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) {
  if (frameEndInfo->layerCount == 0) {
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto d3d11 = this->GetD3D11Device();
  if (!d3d11) {
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto snapshot = mSHM.MaybeGet(d3d11.get(), SHM::ConsumerKind::OpenXR);
  if (!snapshot.IsValid()) {
    // Don't spam: expected, if OpenKneeboard isn't running
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto config = snapshot.GetConfig();
  const auto layerCount = snapshot.GetLayerCount();

  std::vector<const XrCompositionLayerBaseHeader*> nextLayers;
  nextLayers.reserve(frameEndInfo->layerCount + layerCount);
  std::copy(
    frameEndInfo->layers,
    &frameEndInfo->layers[frameEndInfo->layerCount],
    std::back_inserter(nextLayers));

  auto hmdPose = this->GetHMDPose(frameEndInfo->displayTime);

  std::vector<XrCompositionLayerQuad> kneeboardLayers;
  kneeboardLayers.reserve(layerCount);

  uint8_t topMost = layerCount - 1;

  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    auto layer = *snapshot.GetLayerConfig(layerIndex);
    if (!layer.IsValid()) {
      return mOpenXR->xrEndFrame(session, frameEndInfo);
    }

    auto& swapchain = mSwapchains.at(layerIndex);
    if (swapchain) {
      if (!this->ConfigurationsAreCompatible(mInitialConfig, config.mVR)) {
        dprint("Incompatible swapchain due to options change, recreating");
        mOpenXR->xrDestroySwapchain(swapchain);
        swapchain = nullptr;
      }
    }

    if (!swapchain) {
      mInitialConfig = config.mVR;
      swapchain = this->CreateSwapChain(session, mInitialConfig, layerIndex);
      if (!swapchain) {
        dprintf("Failed to create swapchain for layer {}", layerIndex);
        OPENKNEEBOARD_BREAK;
        return mOpenXR->xrEndFrame(session, frameEndInfo);
      }
      dprintf("Created swapchain for layer {}", layerIndex);
    }

    const auto renderParams
      = this->GetRenderParameters(snapshot, layer, hmdPose);
    if (renderParams.mIsLookingAtKneeboard) {
      topMost = layerIndex;
    }
    if (mRenderCacheKeys.at(layerIndex) != renderParams.mCacheKey) {
      if (!this->Render(swapchain, snapshot, layerIndex, renderParams)) {
        dprint("Render failed.");
        OPENKNEEBOARD_BREAK;
        return mOpenXR->xrEndFrame(session, frameEndInfo);
      }
      mRenderCacheKeys.at(layerIndex) = renderParams.mCacheKey;
    }

    static_assert(
      SHM::SHARED_TEXTURE_IS_PREMULTIPLIED,
      "Use premultiplied alpha in shared texture, or pass "
      "XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT");
    kneeboardLayers.push_back({
      .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
      .next = nullptr,
      .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT 
        | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
      .space = mLocalSpace,
      .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
      .subImage = {
        .swapchain = swapchain,
        .imageRect = {
          {0, 0},
          {static_cast<int32_t>(layer.mImageWidth), static_cast<int32_t>(layer.mImageHeight)},
        },
        .imageArrayIndex = 0,
      },
      .pose = this->GetXrPosef(renderParams.mKneeboardPose),
      .size = { renderParams.mKneeboardSize.x, renderParams.mKneeboardSize.y },
    });
    nextLayers.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(&kneeboardLayers.back()));
  }

  if (topMost != layerCount - 1) {
    std::swap(kneeboardLayers.back(), kneeboardLayers.at(topMost));
  }

  XrFrameEndInfo nextFrameEndInfo {*frameEndInfo};
  nextFrameEndInfo.layers = nextLayers.data();
  nextFrameEndInfo.layerCount = static_cast<uint32_t>(nextLayers.size());

  auto nextResult = mOpenXR->xrEndFrame(session, &nextFrameEndInfo);
  if (nextResult != XR_SUCCESS) {
    OPENKNEEBOARD_BREAK;
  }
  return nextResult;
}

OpenXRKneeboard::Pose OpenXRKneeboard::GetHMDPose(XrTime displayTime) {
  static Pose sCache {};
  static XrTime sCacheKey {};
  if (displayTime == sCacheKey) {
    return sCache;
  }

  XrSpaceLocation location {.type = XR_TYPE_SPACE_LOCATION};
  if (
    mOpenXR->xrLocateSpace(mViewSpace, mLocalSpace, displayTime, &location)
    != XR_SUCCESS) {
    return {};
  }

  const auto desiredFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
    | XR_SPACE_LOCATION_POSITION_VALID_BIT;
  if ((location.locationFlags & desiredFlags) != desiredFlags) {
    return {};
  }

  const auto& p = location.pose.position;
  const auto& o = location.pose.orientation;
  sCache = {
    .mPosition = {p.x, p.y, p.z},
    .mOrientation = {o.x, o.y, o.z, o.w},
  };
  sCacheKey = displayTime;
  return sCache;
}

XrPosef OpenXRKneeboard::GetXrPosef(const Pose& pose) {
  const auto& p = pose.mPosition;
  const auto& o = pose.mOrientation;
  return {
    .orientation = {o.x, o.y, o.z, o.w},
    .position = {p.x, p.y, p.z},
  };
}

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
  XrInstanceProperties instanceProps {XR_TYPE_INSTANCE_PROPERTIES};
  gNext->xrGetInstanceProperties(instance, &instanceProps);
  gRuntime.mVersion = instanceProps.runtimeVersion;
  strncpy(gRuntime.mName, instanceProps.runtimeName, XR_MAX_RUNTIME_NAME_SIZE);
  dprintf("OpenXR runtime: '{}' v{:#x}", gRuntime.mName, gRuntime.mVersion);

  const auto ret = gNext->xrCreateSession(instance, createInfo, session);
  if (XR_FAILED(ret)) {
    dprintf("next xrCreateSession failed: {}", static_cast<int>(ret));
    return ret;
  }

  if (gKneeboard) {
    dprint("Already have a kneeboard, refusing to initialize twice");
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard = new OpenXRD3D11Kneeboard(*session, gRuntime, gNext, *d3d11);
    return ret;
  }
  auto d3d12 = findInXrNextChain<XrGraphicsBindingD3D12KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D12_KHR, createInfo->next);
  if (d3d12 && d3d12->device) {
    gKneeboard = new OpenXRD3D12Kneeboard(*session, gRuntime, gNext, *d3d12);
    return ret;
  }

  auto vk = findInXrNextChain<XrGraphicsBindingVulkanKHR>(
    XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, createInfo->next);
  if (vk) {
    if (!gPFN_vkGetInstanceProcAddr) {
      dprint(
        "Found Vulkan, don't have an explicit vkGetInstanceProcAddr; looking "
        "for system library.");
      if (gLibVulkan) {
        gPFN_vkGetInstanceProcAddr
          = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            GetProcAddress(gLibVulkan.get(), "vkGetInstanceProcAddr"));
      }
      if (gPFN_vkGetInstanceProcAddr) {
        dprint("Found usable system vkGetInstanceProcAddr");
      } else {
        dprint("Didn't find usable system vkGetInstanceProcAddr");
        return ret;
      }
    }
    if (!vk->device) {
      dprint("Found Vulkan, but did not find a device");
      return ret;
    }
    if (!gVKAllocator) {
      dprint("Launching Vulkan without a specified allocator");
    }

    gKneeboard = new OpenXRVulkanKneeboard(
      *session, gRuntime, gNext, *vk, gVKAllocator, gPFN_vkGetInstanceProcAddr);
    return ret;
  }

  dprint("Unsupported graphics API");

  return ret;
}

XrResult xrCreateVulkanDeviceKHR(
  XrInstance instance,
  const XrVulkanDeviceCreateInfoKHR* createInfo,
  VkDevice* vulkanDevice,
  VkResult* vulkanResult) {
  dprintf("{}", __FUNCTION__);
  const auto ret = gNext->xrCreateVulkanDeviceKHR(
    instance, createInfo, vulkanDevice, vulkanResult);
  if (XR_SUCCEEDED(ret)) {
    gPFN_vkGetInstanceProcAddr = createInfo->pfnGetInstanceProcAddr;
    gVKAllocator = createInfo->vulkanAllocator;
  }

  return ret;
}

XrResult xrDestroySession(XrSession session) {
  delete gKneeboard;
  gKneeboard = nullptr;
  return gNext->xrDestroySession(session);
}

XrResult xrDestroyInstance(XrInstance instance) {
  delete gKneeboard;
  gKneeboard = nullptr;
  return gNext->xrDestroyInstance(instance);
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
  if (gKneeboard) {
    return gKneeboard->xrEndFrame(session, frameEndInfo);
  }
  return gNext->xrEndFrame(session, frameEndInfo);
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
  if (name == "xrDestroySession") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrDestroySession);
    return XR_SUCCESS;
  }
  if (name == "xrDestroyInstance") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrDestroyInstance);
    return XR_SUCCESS;
  }
  if (name == "xrEndFrame") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrEndFrame);
    return XR_SUCCESS;
  }
  if (name == "xrCreateVulkanDeviceKHR") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrCreateVulkanDeviceKHR);
    return XR_SUCCESS;
  }

  if (gNext) {
    return gNext->xrGetInstanceProcAddr(instance, name_cstr, function);
  }

  if (name == "xrEnumerateApiLayerProperties") {
    // No need to do anything here; should be implemented by the runtime
    return XR_SUCCESS;
  }

  dprintf(
    "Unsupported OpenXR call '{}' with instance {:#016x} and no next",
    name,
    reinterpret_cast<uintptr_t>(instance));
  return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XrResult xrCreateApiLayerInstance(
  const XrInstanceCreateInfo* info,
  const struct XrApiLayerCreateInfo* layerInfo,
  XrInstance* instance) {
  dprintf("{}", __FUNCTION__);
  // TODO: check version fields etc in layerInfo
  XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
  nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
  auto nextResult = layerInfo->nextInfo->nextCreateApiLayerInstance(
    info, &nextLayerInfo, instance);
  if (nextResult != XR_SUCCESS) {
    dprint("Next failed.");
    return nextResult;
  }

  gNext = std::make_shared<OpenXRNext>(
    *instance, layerInfo->nextInfo->nextGetInstanceProcAddr);

  dprint("Created API layer instance");

  return XR_SUCCESS;
}

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.OpenXR")
 * a4308f76-39c8-5a50-4ede-32d104a8a78d
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.OpenXR",
  (0xa4308f76, 0x39c8, 0x5a50, 0x4e, 0xde, 0x32, 0xd1, 0x04, 0xa8, 0xa7, 0x8d));
}// namespace OpenKneeboard

using namespace OpenKneeboard;

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(gTraceProvider);
      break;
    case DLL_PROCESS_DETACH:
      TraceLoggingUnregister(gTraceProvider);
      break;
  }
  return TRUE;
}

extern "C" {
XrResult __declspec(dllexport) XRAPI_CALL
  OpenKneeboard_xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* loaderInfo,
    const char* layerName,
    XrNegotiateApiLayerRequest* apiLayerRequest) {
  DPrintSettings::Set({
    .prefix = "OpenKneeboard-OpenXR",
  });
  dprintf("{} {}", __FUNCTION__, Version::ReleaseName);

  if (layerName != OpenKneeboard::OpenXRLayerName) {
    dprintf("Layer name mismatch:\n -{}\n +{}", OpenXRLayerName, layerName);
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
