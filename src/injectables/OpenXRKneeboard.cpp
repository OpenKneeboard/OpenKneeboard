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
#include <openxr/openxr_reflection.h>

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

constexpr std::string_view OpenXRLayerName {Config::OpenXRApiLayerName};
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

static inline std::string_view xrresult_to_string(XrResult code) {
  // xrResultAsString exists, but isn't reliably giving useful results, e.g.
  // it fails on the Meta XR Simulator.
  switch (code) {
#define XR_RESULT_CASE(enum_name, value) \
  case enum_name: \
    return {#enum_name}; \
    XR_LIST_ENUM_XrResult(XR_RESULT_CASE)
#undef XR_RESULT_CASE
    default:
      return {};
  }
}

static inline XrResult check_xrresult(
  XrResult code,
  const std::source_location& loc = std::source_location::current()) {
  if (XR_SUCCEEDED(code)) [[likely]] {
    return code;
  }

  // xrResultAsString exists, but isn't reliably giving useful results, e.g.
  // it fails on the Meta XR Simulator.
  const auto codeAsString = xrresult_to_string(code);

  if (codeAsString.empty()) {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "OpenXR call failed: {}", static_cast<int>(code));
  } else {
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      loc, "OpenXR call failed: {} ({})", codeAsString, static_cast<int>(code));
  }
}

OpenXRKneeboard::OpenXRKneeboard(
  XrInstance instance,
  XrSystemId system,
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next)
  : mOpenXR(next) {
  dprintf("{}", __FUNCTION__);
  OPENKNEEBOARD_TraceLoggingScope("OpenXRKneeboard::OpenXRKneeboard()");

  XrSystemProperties systemProperties {
    .type = XR_TYPE_SYSTEM_PROPERTIES,
  };
  check_xrresult(
    next->xrGetSystemProperties(instance, system, &systemProperties));
  mMaxLayerCount = systemProperties.graphicsProperties.maxLayerCount;

  dprintf("XR system: {}", std::string_view {systemProperties.systemName});
  // 'Max' appears to be a recommendation for the eyebox:
  // - ignoring it for quads appears to be widely compatible, and is common
  //   practice with other tools
  // - the spec for `XrSwapchainCreateInfo` only says I have to respect the
  //   graphics API's limits, not this one
  // - some runtimes (e.g. SteamVR) have a *really* small size here that
  //   prevents spriting.
  dprintf(
    "System supports up to {} layers, with a suggested resolution of {}x{}",
    mMaxLayerCount,
    systemProperties.graphicsProperties.maxSwapchainImageWidth,
    systemProperties.graphicsProperties.maxSwapchainImageHeight);

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

  mIsVarjoRuntime = std::string_view(runtimeID.mName).starts_with("Varjo");
  if (mIsVarjoRuntime) {
    dprint("Varjo runtime detected");
  }
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

  const auto shm = this->GetSHM();

  if (!(shm && *shm)) {
    TraceLoggingWriteTagged(activity, "No feeder");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto snapshot = shm->MaybeGetMetadata();
  if (!snapshot.HasMetadata()) {
    TraceLoggingWriteTagged(activity, "No metadata");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto availableLayers = this->GetActiveLayers(snapshot);
  const auto layerCount
    = (availableLayers.size() + frameEndInfo->layerCount) < mMaxLayerCount
    ? availableLayers.size()
    : (mMaxLayerCount - frameEndInfo->layerCount);
  if (layerCount == 0) {
    TraceLoggingWriteTagged(activity, "No active layers");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }
  availableLayers.resize(layerCount);
  const auto swapchainDimensions = Spriting::GetBufferSize(layerCount);

  if (mSwapchain) {
    if (
      (mSwapchainDimensions != swapchainDimensions)
      || (mSessionID != snapshot.GetSessionID())) {
      OPENKNEEBOARD_TraceLoggingScope("DestroySwapchain");
      this->ReleaseSwapchainResources(mSwapchain);
      mOpenXR->xrDestroySwapchain(mSwapchain);
      mSwapchain = {};
    }
  }

  if (!mSwapchain) {
    OPENKNEEBOARD_TraceLoggingScope("CreateSwapchain");
    mSwapchain = this->CreateSwapchain(session, swapchainDimensions);
    if (!mSwapchain) [[unlikely]] {
      OPENKNEEBOARD_LOG_AND_FATAL("Failed to create swapchain");
    }
    mSwapchainDimensions = swapchainDimensions;
    mSessionID = snapshot.GetSessionID();
    dprintf(
      "Created {}x{} swapchain",
      swapchainDimensions.mWidth,
      swapchainDimensions.mHeight);
  }

  if (!snapshot.HasTexture()) {
    snapshot = this->GetSHM()->MaybeGet();
  }

  if (!snapshot.HasTexture()) {
    TraceLoggingWriteTagged(activity, "NoTexture");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto config = snapshot.GetConfig();

  std::vector<const XrCompositionLayerBaseHeader*> nextLayers;
  nextLayers.reserve(frameEndInfo->layerCount + layerCount);
  std::copy(
    frameEndInfo->layers,
    &frameEndInfo->layers[frameEndInfo->layerCount],
    std::back_inserter(nextLayers));

  uint8_t topMost = layerCount - 1;

  auto hmdPose = this->GetHMDPose(frameEndInfo->displayTime);

  bool needRender = config.mVR.mQuirks.mOpenXR_AlwaysUpdateSwapchain;
  std::vector<SHM::LayerSprite> layerSprites;
  std::vector<uint64_t> cacheKeys;
  std::vector<XrCompositionLayerQuad> addedXRLayers;
  layerSprites.reserve(layerCount);
  cacheKeys.reserve(layerCount);
  addedXRLayers.reserve(layerCount);

  for (const auto shmLayerIndex: availableLayers) {
    const auto renderLayerIndex = layerSprites.size();

    auto layer = snapshot.GetLayerConfig(shmLayerIndex);
    auto params = this->GetRenderParameters(snapshot, *layer, hmdPose);
    cacheKeys.push_back(params.mCacheKey);
    PixelRect destRect {
      Spriting::GetOffset(renderLayerIndex, MaxViewCount),
      layer->mVR.mLocationOnTexture.mSize,
    };
    using Upscaling = VRConfig::Quirks::Upscaling;
    switch (config.mVR.mQuirks.mOpenXR_Upscaling) {
      case Upscaling::Automatic:
        if (!mIsVarjoRuntime) {
          break;
        }
        [[fallthrough]];
      case Upscaling::AlwaysOn:
        destRect.mSize = destRect.mSize.ScaledToFit(Config::MaxViewRenderSize);
        break;
      case Upscaling::AlwaysOff:
        // nothing to do
        break;
    }

    layerSprites.push_back(SHM::LayerSprite {
      .mSourceRect = layer->mVR.mLocationOnTexture,
      .mDestRect = destRect,
      .mOpacity = params.mKneeboardOpacity,
    });

    if (params.mCacheKey != mRenderCacheKeys.at(shmLayerIndex)) {
      needRender = true;
    }

    if (params.mIsLookingAtKneeboard) {
      topMost = renderLayerIndex;
    }

    static_assert(
      SHM::SHARED_TEXTURE_IS_PREMULTIPLIED,
      "Use premultiplied alpha in shared texture, or pass "
      "XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT");
    addedXRLayers.push_back({
      .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
      .next = nullptr,
      .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT 
        | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
      .space = mLocalSpace,
      .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
      .subImage = XrSwapchainSubImage {
        .swapchain = mSwapchain,
        .imageRect = destRect.StaticCast<int, XrRect2Di>(),
        .imageArrayIndex = 0,
      },
      .pose = this->GetXrPosef(params.mKneeboardPose),
      .size = { params.mKneeboardSize.x, params.mKneeboardSize.y },
    });

    nextLayers.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(&addedXRLayers.back()));
  }

  if (topMost != layerCount - 1) {
    std::swap(addedXRLayers.back(), addedXRLayers.at(topMost));
  }

  if (needRender) {
    uint32_t swapchainTextureIndex;
    {
      OPENKNEEBOARD_TraceLoggingScope("AcquireSwapchainImage");
      check_xrresult(mOpenXR->xrAcquireSwapchainImage(
        mSwapchain, nullptr, &swapchainTextureIndex));
    }

    {
      OPENKNEEBOARD_TraceLoggingScope("WaitSwapchainImage");
      XrSwapchainImageWaitInfo waitInfo {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
        .timeout = XR_INFINITE_DURATION,
      };
      check_xrresult(mOpenXR->xrWaitSwapchainImage(mSwapchain, &waitInfo));
    }

    {
      OPENKNEEBOARD_TraceLoggingScope("RenderLayers()");
      this->RenderLayers(
        mSwapchain, swapchainTextureIndex, snapshot, layerSprites);
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

  XrResult nextResult {};
  {
    OPENKNEEBOARD_TraceLoggingScope("next_xrEndFrame");
    nextResult = mOpenXR->xrEndFrame(session, &nextFrameEndInfo);
  }
  if (!XR_SUCCEEDED(nextResult)) [[unlikely]] {
    const auto codeAsString = xrresult_to_string(nextResult);

    if (codeAsString.empty()) {
      dprintf("next_xrEndFrame() failed: {}", static_cast<int>(nextResult));
    } else {
      dprintf(
        "next_xrEndFrame() failed: {} ({})",
        codeAsString,
        static_cast<int>(nextResult));
    }
  }
  return nextResult;
}

std::vector<uint8_t> OpenXRKneeboard::GetActiveLayers(
  const SHM::Snapshot& snapshot) const {
  const auto totalLayers = snapshot.GetLayerCount();

  std::vector<uint8_t> ret;
  ret.reserve(totalLayers);

  for (uint32_t layerIndex = 0; layerIndex < totalLayers; ++layerIndex) {
    auto config = snapshot.GetLayerConfig(layerIndex);
    if (config->mVREnabled) {
      ret.push_back(layerIndex);
    }
  }

  return ret;
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

  const auto system = createInfo->systemId;

  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard = new OpenXRD3D11Kneeboard(
      instance, system, *session, gRuntime, gNext, *d3d11);
    return ret;
  }
  auto d3d12 = findInXrNextChain<XrGraphicsBindingD3D12KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D12_KHR, createInfo->next);
  if (d3d12 && d3d12->device) {
    gKneeboard = new OpenXRD3D12Kneeboard(
      instance, system, *session, gRuntime, gNext, *d3d12);
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
          "WARNING: XR_KHR_vulkan_enable2 was used for instance creation, "
          "but not device; unsupported");
        return ret;
      case VulkanXRState::VKEnable2InstanceAndDevice:
        dprint(
          "GOOD: XR_KHR_vulkan_enable2 used for instance and device "
          "creation");
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
      instance,
      system,
      *session,
      gRuntime,
      gNext,
      *vk,
      gVKAllocator,
      gPFN_vkGetInstanceProcAddr);
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
