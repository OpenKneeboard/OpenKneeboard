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

#include "OpenXRD3D11Kneeboard.h"
#include "OpenXRD3D12Kneeboard.h"
#include "OpenXRNext.h"
#include "OpenXRVulkanKneeboard.h"

#include <OpenKneeboard/Elevation.h>
#include <OpenKneeboard/Spriting.h>
#include <OpenKneeboard/StateMachine.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

#include <loader_interfaces.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace OpenKneeboard {

namespace {
enum class VulkanXRState {
  NoVKEnable2,
  VKEnable2Instance,
  VKEnable2InstanceAndDevice,
};
}
OPENKNEEBOARD_DECLARE_STATE_TRANSITION(
  VulkanXRState::NoVKEnable2,
  VulkanXRState::VKEnable2Instance);
OPENKNEEBOARD_DECLARE_STATE_TRANSITION(
  VulkanXRState::VKEnable2Instance,
  VulkanXRState::VKEnable2InstanceAndDevice);
static StateMachine gVulkanXRState(VulkanXRState::NoVKEnable2);

static constexpr XrPosef XR_POSEF_IDENTITY {
  .orientation = {0.0f, 0.0f, 0.0f, 1.0f},
  .position = {0.0f, 0.0f, 0.0f},
};

static inline XrResult check_xrresult(
  XrResult code,
  const std::source_location& loc = std::source_location::current()) {
  if (XR_FAILED(code)) {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "OpenXR call failed: {}", static_cast<int>(code));
  }
  return code;
}

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
  OPENKNEEBOARD_TraceLoggingScope("OpenXRKneeboard::OpenXRKneeboard()");

  mRenderCacheKeys.fill(~(0ui64));

  XrReferenceSpaceCreateInfo referenceSpace {
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .next = nullptr,
    .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
    .poseInReferenceSpace = XR_POSEF_IDENTITY,
  };

  auto oxr = next.get();

  check_xrresult(
    oxr->xrCreateReferenceSpace(session, &referenceSpace, &mLocalSpace));

  referenceSpace.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  check_xrresult(
    oxr->xrCreateReferenceSpace(session, &referenceSpace, &mViewSpace));
}

OpenXRKneeboard::~OpenXRKneeboard() {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRKneeboard::OpenXRKneeboard()");

  if (mLocalSpace) {
    mOpenXR->xrDestroySpace(mLocalSpace);
  }
  if (mViewSpace) {
    mOpenXR->xrDestroySpace(mViewSpace);
  }

  if (mSwapchain) {
    mOpenXR->xrDestroySwapchain(mSwapchain);
  }
}

OpenXRNext* OpenXRKneeboard::GetOpenXR() {
  return mOpenXR.get();
}

XrResult OpenXRKneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "OpenXRKneeboard::xrEndFrame()");
  if (frameEndInfo->layerCount == 0) {
    TraceLoggingWriteTagged(activity, "No layers.");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  if (!*this->GetSHM()) {
    TraceLoggingWriteTagged(activity, "No feeder");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  if (!mSwapchain) {
    OPENKNEEBOARD_TraceLoggingScope("Create swapchain");
    const auto size = Spriting::GetBufferSize(MaxLayers);

    mSwapchain = this->CreateSwapchain(session, size);
    if (!mSwapchain) [[unlikely]] {
      OPENKNEEBOARD_LOG_AND_FATAL("Failed to create swapchain");
    }
    dprintf("Created {}x{} swapchain", size.mWidth, size.mHeight);
  }

  auto snapshot = this->GetSHM()->MaybeGet();
  if (!snapshot.IsValid()) {
    TraceLoggingWriteTagged(activity, "no snapshot");
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

  bool needRender = config.mVR.mQuirks.mOpenXR_AlwaysUpdateSwapchain;
  std::vector<PixelRect> destRects;
  std::vector<float> opacities;
  std::vector<uint64_t> cacheKeys;
  destRects.reserve(layerCount);
  opacities.reserve(layerCount);
  cacheKeys.reserve(layerCount);

  for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    auto layer = snapshot.GetLayerConfig(layerIndex);
    auto params = this->GetRenderParameters(snapshot, *layer, hmdPose);
    cacheKeys.push_back(params.mCacheKey);
    destRects.push_back(
      {Spriting::GetOffset(layerIndex, MaxLayers),
       layer->mLocationOnTexture.mSize});
    opacities.push_back(params.mKneeboardOpacity);

    if (params.mCacheKey != mRenderCacheKeys.at(layerIndex)) {
      needRender = true;
    }

    if (params.mIsLookingAtKneeboard) {
      topMost = layerIndex;
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
      .subImage = XrSwapchainSubImage {
        .swapchain = mSwapchain,
        .imageRect = {
          {layerIndex * TextureWidth, 0},
          layer->mLocationOnTexture.mSize.StaticCast<XrExtent2Di, int32_t>(),
        },
        .imageArrayIndex = 0,
      },
      .pose = this->GetXrPosef(params.mKneeboardPose),
      .size = { params.mKneeboardSize.x, params.mKneeboardSize.y },
    });

    nextLayers.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(&kneeboardLayers.back()));
  }

  if (topMost != layerCount - 1) {
    std::swap(kneeboardLayers.back(), kneeboardLayers.at(topMost));
  }

  if (needRender) {
    uint32_t swapchainTextureIndex;
    check_xrresult(mOpenXR->xrAcquireSwapchainImage(
      mSwapchain, nullptr, &swapchainTextureIndex));

    XrSwapchainImageWaitInfo waitInfo {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
    };
    check_xrresult(mOpenXR->xrWaitSwapchainImage(mSwapchain, &waitInfo));

    {
      OPENKNEEBOARD_TraceLoggingScope("RenderLayers()");
      this->RenderLayers(
        mSwapchain,
        swapchainTextureIndex,
        snapshot,
        destRects.data(),
        opacities.data());
    }

    {
      OPENKNEEBOARD_TraceLoggingScope("xrReleaseSwapchainImage()");
      check_xrresult(mOpenXR->xrReleaseSwapchainImage(mSwapchain, nullptr));
    }

    for (size_t i = 0; i < cacheKeys.size(); ++i) {
      mRenderCacheKeys[i] = cacheKeys.at(i);
    }
  }

  XrFrameEndInfo nextFrameEndInfo {*frameEndInfo};
  nextFrameEndInfo.layers = nextLayers.data();
  nextFrameEndInfo.layerCount = static_cast<uint32_t>(nextLayers.size());

  XrResult nextResult;
  {
    OPENKNEEBOARD_TraceLoggingScope("next_xrEndFrame");
    nextResult
      = check_xrresult(mOpenXR->xrEndFrame(session, &nextFrameEndInfo));
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
  XrSession* session) noexcept {
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

  auto vk = findInXrNextChain<XrGraphicsBindingVulkan2KHR>(
    XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, createInfo->next);
  if (vk) {
    switch (gVulkanXRState.Get()) {
      case VulkanXRState::NoVKEnable2:
        dprint(
          "WARNING: Got an XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, but "
          "XR_KHR_vulkan_enable2 instance/device creation functions were not "
          "used; unsupported");
        return ret;
      case VulkanXRState::VKEnable2Instance:
        dprint(
          "WARNING: XR_KHR_vulkan_enable2 was used for instance creation, but "
          "not "
          "device; unsupported");
        return ret;
      case VulkanXRState::VKEnable2InstanceAndDevice:
        dprint(
          "GOOD: XR_KHR_vulkan_enable2 used for instance and device creation");
        break;
      default:
        dprintf(
          "ERROR: Unrecognized VulkanXRState: {}",
          static_cast<std::underlying_type_t<VulkanXRState>>(
            gVulkanXRState.Get()));
        OPENKNEEBOARD_BREAK;
        return ret;
    }
    if (!gPFN_vkGetInstanceProcAddr) {
      dprint(
        "Found Vulkan, don't have an explicit vkGetInstanceProcAddr; "
        "looking "
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

// Provided by XR_KHR_vulkan_enable2
XrResult xrCreateVulkanInstanceKHR(
  XrInstance instance,
  const XrVulkanInstanceCreateInfoKHR* origCreateInfo,
  VkInstance* vulkanInstance,
  VkResult* vulkanResult) {
  dprintf("{}()", __FUNCTION__);

  const OpenXRVulkanKneeboard::VkInstanceCreateInfo vkCreateInfo(
    *origCreateInfo->vulkanCreateInfo);

  auto createInfo = *origCreateInfo;
  createInfo.vulkanCreateInfo = &vkCreateInfo;

  const auto ret = gNext->xrCreateVulkanInstanceKHR(
    instance, &createInfo, vulkanInstance, vulkanResult);

  if (createInfo.pfnGetInstanceProcAddr) {
    gPFN_vkGetInstanceProcAddr = createInfo.pfnGetInstanceProcAddr;
  }
  if (createInfo.vulkanAllocator) {
    gVKAllocator = createInfo.vulkanAllocator;
  }

  gVulkanXRState
    .Transition<VulkanXRState::NoVKEnable2, VulkanXRState::VKEnable2Instance>();

  return ret;
}

// Provided by XR_KHR_vulkan_enable2
XrResult xrCreateVulkanDeviceKHR(
  XrInstance instance,
  const XrVulkanDeviceCreateInfoKHR* origCreateInfo,
  VkDevice* vulkanDevice,
  VkResult* vulkanResult) {
  dprintf("{}()", __FUNCTION__);

  const OpenXRVulkanKneeboard::VkDeviceCreateInfo vkCreateInfo(
    *origCreateInfo->vulkanCreateInfo);

  auto createInfo = *origCreateInfo;
  createInfo.vulkanCreateInfo = &vkCreateInfo;
  const auto ret = gNext->xrCreateVulkanDeviceKHR(
    instance, &createInfo, vulkanDevice, vulkanResult);
  if (XR_FAILED(ret)) {
    return ret;
  }

  if (createInfo.pfnGetInstanceProcAddr) {
    gPFN_vkGetInstanceProcAddr = createInfo.pfnGetInstanceProcAddr;
  }
  if (createInfo.vulkanAllocator) {
    gVKAllocator = createInfo.vulkanAllocator;
  }

  gVulkanXRState.Transition<
    VulkanXRState::VKEnable2Instance,
    VulkanXRState::VKEnable2InstanceAndDevice>();

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

XrResult xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) noexcept {
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

  ///// START XR_KHR_vulkan_enable2 /////
  if (name == "xrCreateVulkanDeviceKHR") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(&xrCreateVulkanDeviceKHR);
    return XR_SUCCESS;
  }
  if (name == "xrCreateVulkanInstanceKHR") {
    *function
      = reinterpret_cast<PFN_xrVoidFunction>(&xrCreateVulkanInstanceKHR);
    return XR_SUCCESS;
  }
  ///// END XR_KHR_vulkan_enable2 /////

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
      DPrintSettings::Set({
        .prefix = "OpenKneeboard-OpenXR",
      });
      dprintf(
        "{} {}, {}",
        __FUNCTION__,
        Version::ReleaseName,
        IsElevated(GetCurrentProcess()) ? "elevated" : "not elevated");
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
  dprintf("{}", __FUNCTION__);

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
