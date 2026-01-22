// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRKneeboard.hpp"

#include "OpenXRD3D11Kneeboard.hpp"
#include "OpenXRD3D12Kneeboard.hpp"
#include "OpenXRNext.hpp"
#include "OpenXRVulkanKneeboard.hpp"

#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Spriting.hpp>
#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/vulkan/vulkan.h>

#include <memory>
#include <string>

#include <loader_interfaces.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <shims/openxr/openxr.h>

#include <wil/resource.h>

#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>

namespace OpenKneeboard {

namespace {
// TODO: these should all be part of the instance, and reset when a new instance
// is created

enum class VulkanXRStates {
  NoVKEnable2,
  VKEnable2Instance,
  VKEnable2InstanceAndDevice,
};

auto& VulkanXRState() {
  static StateMachine<
    VulkanXRStates,
    VulkanXRStates::NoVKEnable2,
    std::array {
      Transition {
        VulkanXRStates::NoVKEnable2,
        VulkanXRStates::VKEnable2Instance,
      },
      Transition {
        VulkanXRStates::VKEnable2Instance,
        VulkanXRStates::VKEnable2InstanceAndDevice,
      },
    }>
    state;
  return state;
}

bool gHaveXR_KHR_vulkan_enable2 {false};
}// namespace

static constexpr XrPosef XR_POSEF_IDENTITY {
  .orientation = {0.0f, 0.0f, 0.0f, 1.0f},
  .position = {0.0f, 0.0f, 0.0f},
};

static_assert(OpenXRApiLayerName.size() <= XR_MAX_API_LAYER_NAME_SIZE);
static_assert(
  OpenXRApiLayerDescription.size() <= XR_MAX_API_LAYER_DESCRIPTION_SIZE);

// Don't use a unique_ptr as on process exit, windows doesn't clean these up
// in a useful order, and Microsoft recommend just leaking resources on
// thread/process exit
//
// In this case, it leads to an infinite hang on ^C
static OpenXRKneeboard* gKneeboard {nullptr};

auto& Next() {
  static std::shared_ptr<OpenXRNext> it;
  return it;
}
static OpenXRRuntimeID gRuntime {};
static PFN_vkGetInstanceProcAddr gPFN_vkGetInstanceProcAddr {nullptr};
static const VkAllocationCallbacks* gVKAllocator {nullptr};

static HMODULE LibVulkan() {
  static wil::unique_hmodule data {LoadLibraryW(L"vulkan-1.dll")};
  return data.get();
}

static std::string_view xrresult_to_string(const XrResult code) {
  // xrResultAsString exists, but isn't reliably giving useful results, e.g.
  // it fails on the Meta XR Simulator.
  switch (code) {
#define XR_RESULT_CASE(enum_name, value) \
  case enum_name: \
    return {#enum_name};
    XR_LIST_ENUM_XrResult(XR_RESULT_CASE)
#undef XR_RESULT_CASE
      default : return {};
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
    fatal(loc, "OpenXR call failed: {}", static_cast<int>(code));
  } else {
    fatal(
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
  dprint("{}", __FUNCTION__);
  OPENKNEEBOARD_TraceLoggingScope("OpenXRKneeboard::OpenXRKneeboard()");

  XrSystemProperties systemProperties {
    .type = XR_TYPE_SYSTEM_PROPERTIES,
  };
  check_xrresult(
    next->xrGetSystemProperties(instance, system, &systemProperties));
  mMaxLayerCount = systemProperties.graphicsProperties.maxLayerCount;

  dprint("XR system: {}", std::string_view {systemProperties.systemName});
  // 'Max' appears to be a recommendation for the eyebox:
  // - ignoring it for quads appears to be widely compatible, and is common
  //   practice with other tools
  // - the spec for `XrSwapchainCreateInfo` only says I have to respect the
  //   graphics API's limits, not this one
  // - some runtimes (e.g. SteamVR) have a *really* small size here that
  //   prevents spriting.
  dprint(
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

OpenXRNext* OpenXRKneeboard::GetOpenXR() { return mOpenXR.get(); }

XrResult OpenXRKneeboard::xrEndFrame(
  XrSession session,
  const XrFrameEndInfo* frameEndInfo) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "OpenXRKneeboard::xrEndFrame()");
  if (frameEndInfo->layerCount == 0) {
    TraceLoggingWriteTagged(activity, "No game layers.");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto& shm = this->GetSHM();

  if (!shm) {
    TraceLoggingWriteTagged(activity, "No SHM");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto frame = shm.MaybeGet();
  if (!frame) {
    TraceLoggingWriteTagged(
      activity,
      "No frame",
      OPENKNEEBOARD_TraceLoggingStringView(
        magic_enum::enum_name(frame.error()), "Error"));
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  if (frame->mLayers.empty()) {
    TraceLoggingWriteTagged(activity, "No SHM layers");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  const auto swapchainDimensions =
    Spriting::GetBufferSize(frame->mLayers.size());

  if (mSwapchain && mSwapchainDimensions != swapchainDimensions) {
    OPENKNEEBOARD_TraceLoggingScope("DestroySwapchain");
    this->ReleaseSwapchainResources(mSwapchain);
    mOpenXR->xrDestroySwapchain(mSwapchain);
    mSwapchain = {};
  }

  if (!mSwapchain) {
    OPENKNEEBOARD_TraceLoggingScope(
      "CreateSwapchain",
      TraceLoggingValue(swapchainDimensions.mWidth, "width"),
      TraceLoggingValue(swapchainDimensions.mHeight, "height"));
    mSwapchain = this->CreateSwapchain(session, swapchainDimensions);
    if (!mSwapchain) [[unlikely]] {
      fatal("Failed to create swapchain");
    }
    mSwapchainDimensions = swapchainDimensions;
    dprint(
      "Created {}x{} swapchain",
      swapchainDimensions.mWidth,
      swapchainDimensions.mHeight);
  }

  const auto hmdPose = this->GetHMDPose(frameEndInfo->displayTime);
  auto vrLayers = this->GetLayers(frame->mConfig, frame->mLayers, hmdPose);
  const auto layerCount =
    (vrLayers.size() + frameEndInfo->layerCount) <= mMaxLayerCount
    ? vrLayers.size()
    : (mMaxLayerCount - frameEndInfo->layerCount);
  if (layerCount == 0) {
    TraceLoggingWriteTagged(activity, "No active layers");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }
  vrLayers.resize(layerCount);

  std::vector<const XrCompositionLayerBaseHeader*> nextLayers;
  nextLayers.reserve(frameEndInfo->layerCount + layerCount);
  std::copy(
    frameEndInfo->layers,
    &frameEndInfo->layers[frameEndInfo->layerCount],
    std::back_inserter(nextLayers));

  uint8_t topMost = layerCount - 1;

  std::vector<SHM::LayerSprite> layerSprites;
  std::vector<XrCompositionLayerQuad> addedXRLayers;
  layerSprites.reserve(layerCount);
  addedXRLayers.reserve(layerCount);

  for (size_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    const auto [layer, params] = vrLayers.at(layerIndex);
    OPENKNEEBOARD_TraceLoggingScopedActivity(
      layerActivity,
      "OpenXRKneeboard::xrEndFrame()/LayerSprites",
      TraceLoggingValue(layerIndex, "LayerIndex"),
      TraceLoggingHexUInt64(layer->mLayerID, "LayerID"),
      OPENKNEEBOARD_TraceLoggingRect(
        layer->mVR.mLocationOnTexture, "LocationOnTexture"));

    PixelRect destRect {
      Spriting::GetOffset(layerIndex, frame->mLayers.size()),
      layer->mVR.mLocationOnTexture.mSize,
    };
    using Upscaling = VRSettings::Quirks::Upscaling;
    switch (frame->mConfig.mVR.mQuirks.mOpenXR_Upscaling) {
      case Upscaling::Automatic:
        if (!mIsVarjoRuntime) {
          break;
        }
        [[fallthrough]];
      case Upscaling::AlwaysOn: {
        const auto upscaled =
          destRect.mSize.ScaledToFit(Config::MaxViewRenderSize);
        TraceLoggingWriteTagged(
          layerActivity,
          "OpenXRKneeboard::xrEndFrame()/LayerSprites/Upscale",
          OPENKNEEBOARD_TraceLoggingSize2D(destRect.mSize, "OriginalSize"),
          OPENKNEEBOARD_TraceLoggingSize2D(upscaled, "NewSize"));
        destRect.mSize = upscaled;
        break;
      }
      case Upscaling::AlwaysOff:
        // nothing to do
        break;
    }

    layerSprites.push_back(
      SHM::LayerSprite {
        .mSourceRect = layer->mVR.mLocationOnTexture,
        .mDestRect = destRect,
        .mOpacity = params.mKneeboardOpacity,
      });

    TraceLoggingWriteTagged(
      layerActivity,
      "OpenXRKneeboard::xrEndFrame()/LayerSprites/Sprite",
      OPENKNEEBOARD_TraceLoggingRect(
        layerSprites.back().mSourceRect, "SourceRect"),
      OPENKNEEBOARD_TraceLoggingRect(layerSprites.back().mDestRect, "DestRect"),
      TraceLoggingValue(layerSprites.back().mOpacity, "Opacity"));

    if (layer->mLayerID == frame->mConfig.mGlobalInputLayerID) {
      topMost = layerIndex;
    }

    static_assert(
      SHM::SHARED_TEXTURE_IS_PREMULTIPLIED,
      "Use premultiplied alpha in shared texture, or pass "
      "XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT");
    addedXRLayers.push_back({
      .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
      .next = nullptr,
      .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
      .space = mLocalSpace,
      .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
      .subImage =
        XrSwapchainSubImage {
          .swapchain = mSwapchain,
          .imageRect = destRect.StaticCast<int, XrRect2Di>(),
          .imageArrayIndex = 0,
        },
      .pose = this->GetXrPosef(params.mKneeboardPose),
      .size = {params.mKneeboardSize.x, params.mKneeboardSize.y},
    });

    nextLayers.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(&addedXRLayers.back()));
  }

  if (topMost != layerCount - 1) {
    std::swap(addedXRLayers.back(), addedXRLayers.at(topMost));
  }

  uint32_t swapchainTextureIndex {~(0ui32)};
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
      mSwapchain, swapchainTextureIndex, *std::move(frame), layerSprites);
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("xrReleaseSwapchainImage()");
    check_xrresult(mOpenXR->xrReleaseSwapchainImage(mSwapchain, nullptr));
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
      dprint("next_xrEndFrame() failed: {}", static_cast<int>(nextResult));
    } else {
      dprint(
        "next_xrEndFrame() failed: {} ({})",
        codeAsString,
        static_cast<int>(nextResult));
    }
  }
  return nextResult;
}

OpenXRKneeboard::Pose OpenXRKneeboard::GetHMDPose(XrTime displayTime) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity,
    "OpenXRKNeeboard::GetHMDPose()",
    TraceLoggingValue(displayTime, "displayTime"));
  static Pose sCache {};
  static XrTime sCacheKey {};
  if (displayTime == sCacheKey) {
    activity.StopWithResult("Using Cached Value");
    return sCache;
  }

  XrSpaceLocation location {.type = XR_TYPE_SPACE_LOCATION};
  const auto locateResult =
    mOpenXR->xrLocateSpace(mViewSpace, mLocalSpace, displayTime, &location);
  if (locateResult != XR_SUCCESS) {
    TraceLoggingWriteStop(
      activity,
      "OpenXRKneeboard::GetHMDPose()",
      TraceLoggingValue("locateFailed", "result"),
      TraceLoggingValue(static_cast<int64_t>(locateResult), "xrResult"),
      TraceLoggingValue(
        std::string(xrresult_to_string(locateResult)).c_str(),
        "xrResultString"));
    activity.CancelAutoStop();
    return {};
  }

  const auto desiredFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
    | XR_SPACE_LOCATION_POSITION_VALID_BIT;
  if ((location.locationFlags & desiredFlags) != desiredFlags) {
    activity.StopWithResult("missingFlags");
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

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(
  XrInstance instance,
  const XrSessionCreateInfo* createInfo,
  XrSession* session) noexcept {
  XrInstanceProperties instanceProps {XR_TYPE_INSTANCE_PROPERTIES};
  Next()->xrGetInstanceProperties(instance, &instanceProps);
  gRuntime.mVersion = instanceProps.runtimeVersion;
  strncpy_s(
    gRuntime.mName,
    sizeof(gRuntime.mName),
    instanceProps.runtimeName,
    XR_MAX_RUNTIME_NAME_SIZE);
  dprint("OpenXR runtime: '{}' v{:#x}", gRuntime.mName, gRuntime.mVersion);

  const auto ret = Next()->xrCreateSession(instance, createInfo, session);
  if (XR_FAILED(ret)) {
    dprint("next xrCreateSession failed: {}", static_cast<int>(ret));
    return ret;
  }

  if (gKneeboard) {
    dprint("Already have a kneeboard, refusing to initialize twice");
    return XR_ERROR_LIMIT_REACHED;
  }

  const auto system = createInfo->systemId;

  auto d3d11 = findInXrNextChain<XrGraphicsBindingD3D11KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, createInfo->next);
  if (d3d11 && d3d11->device) {
    gKneeboard = new OpenXRD3D11Kneeboard(
      instance, system, *session, gRuntime, Next(), *d3d11);
    return ret;
  }
  auto d3d12 = findInXrNextChain<XrGraphicsBindingD3D12KHR>(
    XR_TYPE_GRAPHICS_BINDING_D3D12_KHR, createInfo->next);
  if (d3d12 && d3d12->device) {
    gKneeboard = new OpenXRD3D12Kneeboard(
      instance, system, *session, gRuntime, Next(), *d3d12);
    return ret;
  }

  auto vk = findInXrNextChain<XrGraphicsBindingVulkan2KHR>(
    XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, createInfo->next);
  if (vk) {
    switch (const auto s = VulkanXRState().Get()) {
      case VulkanXRStates::NoVKEnable2:
        dprint.Warning(
          "Got an XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, but "
          "XR_KHR_vulkan_enable2 instance/device creation functions were not "
          "used; unsupported");
        return ret;
      case VulkanXRStates::VKEnable2Instance:
        dprint.Warning(
          "XR_KHR_vulkan_enable2 was used for instance creation, "
          "but not device; unsupported");
        return ret;
      case VulkanXRStates::VKEnable2InstanceAndDevice:
        dprint(
          "GOOD: XR_KHR_vulkan_enable2 used for instance and device "
          "creation");
        break;
      default:
        dprint.Error("Unrecognized VulkanXRState: {}", std::to_underlying(s));
        OPENKNEEBOARD_BREAK;
        return ret;
    }
    if (!gPFN_vkGetInstanceProcAddr) {
      dprint(
        "Found Vulkan, don't have an explicit vkGetInstanceProcAddr; "
        "looking "
        "for system library.");
      if (LibVulkan()) {
        gPFN_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(
          GetProcAddress(LibVulkan(), "vkGetInstanceProcAddr"));
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
      Next(),
      *vk,
      gVKAllocator,
      gPFN_vkGetInstanceProcAddr);
    return ret;
  }

  dprint("Unsupported graphics API");

  return ret;
}

// Provided by XR_KHR_vulkan_enable2
XRAPI_ATTR XrResult XRAPI_CALL xrCreateVulkanInstanceKHR(
  XrInstance instance,
  const XrVulkanInstanceCreateInfoKHR* origCreateInfo,
  VkInstance* vulkanInstance,
  VkResult* vulkanResult) {
  dprint("{}()", __FUNCTION__);

  const OpenXRVulkanKneeboard::VkInstanceCreateInfo vkCreateInfo(
    *origCreateInfo->vulkanCreateInfo);

  auto createInfo = *origCreateInfo;
  createInfo.vulkanCreateInfo = &vkCreateInfo;

  const auto ret = Next()->xrCreateVulkanInstanceKHR(
    instance, &createInfo, vulkanInstance, vulkanResult);

  if (createInfo.pfnGetInstanceProcAddr) {
    gPFN_vkGetInstanceProcAddr = createInfo.pfnGetInstanceProcAddr;
  }
  if (createInfo.vulkanAllocator) {
    gVKAllocator = createInfo.vulkanAllocator;
  }

  VulkanXRState()
    .Transition<
      VulkanXRStates::NoVKEnable2,
      VulkanXRStates::VKEnable2Instance>();

  return ret;
}

// Provided by XR_KHR_vulkan_enable2
XRAPI_ATTR XrResult XRAPI_CALL xrCreateVulkanDeviceKHR(
  XrInstance instance,
  const XrVulkanDeviceCreateInfoKHR* origCreateInfo,
  VkDevice* vulkanDevice,
  VkResult* vulkanResult) {
  dprint("{}()", __FUNCTION__);

  const OpenXRVulkanKneeboard::VkDeviceCreateInfo vkCreateInfo(
    *origCreateInfo->vulkanCreateInfo);

  auto createInfo = *origCreateInfo;
  createInfo.vulkanCreateInfo = &vkCreateInfo;
  const auto ret = Next()->xrCreateVulkanDeviceKHR(
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

  VulkanXRState()
    .Transition<
      VulkanXRStates::VKEnable2Instance,
      VulkanXRStates::VKEnable2InstanceAndDevice>();

  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession session) {
  delete gKneeboard;
  gKneeboard = nullptr;
  return Next()->xrDestroySession(session);
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(XrInstance instance) {
  delete gKneeboard;
  gKneeboard = nullptr;
  return Next()->xrDestroyInstance(instance);
}

XRAPI_ATTR XrResult XRAPI_CALL
xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) noexcept {
  if (gKneeboard) {
    return gKneeboard->xrEndFrame(session, frameEndInfo);
  }
  return Next()->xrEndFrame(session, frameEndInfo);
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateApiLayerProperties(
  uint32_t propertyCapacityInput,
  uint32_t* propertyCountOutput,
  XrApiLayerProperties* properties) {
  // We only return our own properties per spec:
  // https://registry.khronos.org/OpenXR/specs/1.0/loader.html#api-layer-conventions-and-rules

  *propertyCountOutput = 1;
  if (propertyCapacityInput == 0) {
    // Do not return XR_ERROR_SIZE_SUFFICIENT for 0, per
    // https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#buffer-size-parameters
    return XR_SUCCESS;
  }

  if (properties->type != XR_TYPE_API_LAYER_PROPERTIES) {
    return XR_ERROR_VALIDATION_FAILURE;
  }

  memset(properties->layerName, 0, std::size(properties->layerName));
  memcpy_s(
    properties->layerName,
    std::size(properties->layerName),
    OpenXRApiLayerName.data(),
    OpenXRApiLayerName.size());

  properties->specVersion = XR_CURRENT_API_VERSION;

  properties->layerVersion = Version::OpenXRApiLayerImplementationVersion;

  memcpy_s(
    properties->description,
    std::size(properties->description),
    OpenXRApiLayerDescription.data(),
    OpenXRApiLayerDescription.size());

  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(
  const char* layerName,
  uint32_t propertyCapacityInput,
  uint32_t* propertyCountOutput,
  XrExtensionProperties* properties) {
  // This is installed as an implicit layer; for behavior spec, see:
  // https://registry.khronos.org/OpenXR/specs/1.0/loader.html#api-layer-conventions-and-rules
  if (layerName && layerName == OpenXRApiLayerName) {
    *propertyCountOutput = 0;
    // We don't implement any instance extensions
    return XR_SUCCESS;
  }

  // As we don't implement any extensions, just delegate to the runtime or next
  // layer.
  if (Next() && Next()->xrEnumerateInstanceExtensionProperties) {
    return Next()->xrEnumerateInstanceExtensionProperties(
      layerName, propertyCapacityInput, propertyCountOutput, properties);
  }

  if (layerName) {
    // If layerName is non-null and not our layer, it should be an earlier
    // layer, or we should have a `next`
    return XR_ERROR_API_LAYER_NOT_PRESENT;
  }

  // for NULL layerName, we append our list to the next; as we have none, that
  // just means we have 0 again
  *propertyCountOutput = 0;
  return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
  XrInstance instance,
  const char* name_cstr,
  PFN_xrVoidFunction* function) {
  const std::string_view name {name_cstr};

#define HOOK_FUNC(x) \
  if (name == #x) { \
    *function = reinterpret_cast<PFN_xrVoidFunction>(&x); \
    return XR_SUCCESS; \
  }
#define HOOK_EXT_FUNC(ext, func) \
  if (name == #func) { \
    if (gHave##ext) [[likely]] { \
      *function = reinterpret_cast<PFN_xrVoidFunction>(&func); \
      return XR_SUCCESS; \
    } \
    return XR_ERROR_FUNCTION_UNSUPPORTED; \
  }
  OPENKNEEBOARD_HOOKED_OPENXR_FUNCS(HOOK_FUNC, HOOK_EXT_FUNC)
#undef HOOK_FUNC
#undef HOOK_EXT_FUNC

  if (name == "xrEnumerateApiLayerProperties") {
    *function =
      reinterpret_cast<PFN_xrVoidFunction>(&xrEnumerateApiLayerProperties);
    return XR_SUCCESS;
  }

  if (name == "xrEnumerateInstanceExtensionProperties") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(
      &xrEnumerateInstanceExtensionProperties);
    return XR_SUCCESS;
  }

  if (const auto next = Next().get()) {
    return next->xrGetInstanceProcAddr(instance, name_cstr, function);
  }

  dprint(
    "Unsupported OpenXR call '{}' with instance {:#018x} and no next",
    name,
    std::bit_cast<uint64_t>(instance));
  return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static std::string_view BoundedStringView(
  char const* p,
  const std::size_t maxLength) {
  return {p, strnlen_s(p, maxLength)};
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateApiLayerInstance(
  const XrInstanceCreateInfo* info,
  const struct XrApiLayerCreateInfo* layerInfo,
  XrInstance* instance) {
  dprint(
    "{} - Application `{}` (v{}), engine `{}` (v{})",
    __FUNCTION__,
    BoundedStringView(
      info->applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE),
    info->applicationInfo.applicationVersion,
    BoundedStringView(
      info->applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE),
    info->applicationInfo.engineVersion);
  // TODO: check version fields etc in layerInfo
  XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
  nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
  const auto nextResult = layerInfo->nextInfo->nextCreateApiLayerInstance(
    info, &nextLayerInfo, instance);
  if (XR_FAILED(nextResult)) {
    dprint("Next failed.");
    return nextResult;
  }

  gHaveXR_KHR_vulkan_enable2 = false;

  for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
    const std::string_view extensionName {info->enabledExtensionNames[i]};
    dprint("Application enabled extension: {}", extensionName);
    if (extensionName == XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) {
      gHaveXR_KHR_vulkan_enable2 = true;
      continue;
    }
  }

  Next() = std::make_shared<OpenXRNext>(
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

BOOL WINAPI DllMain(HINSTANCE, const DWORD dwReason, LPVOID /*lpReserved*/) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(gTraceProvider);
      DPrintSettings::Set({
        .prefix = "OpenKneeboard-OpenXR",
      });
      dprint(
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

extern "C" XRAPI_ATTR XrResult XRAPI_CALL
OpenKneeboard_xrNegotiateLoaderApiLayerInterface(
  const XrNegotiateLoaderInfo*,
  const char* layerName,
  XrNegotiateApiLayerRequest* apiLayerRequest) {
  dprint("{}", __FUNCTION__);

  if (layerName != OpenXRApiLayerName) {
    dprint("Layer name mismatch:\n -{}\n +{}", OpenXRApiLayerName, layerName);
    return XR_ERROR_INITIALIZATION_FAILED;
  }

  // TODO: check version fields etc in loaderInfo

  apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
  apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
  apiLayerRequest->getInstanceProcAddr = &OpenKneeboard::xrGetInstanceProcAddr;
  apiLayerRequest->createApiLayerInstance =
    &OpenKneeboard::xrCreateApiLayerInstance;
  return XR_SUCCESS;
}
