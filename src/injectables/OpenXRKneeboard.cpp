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

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>
#include <OpenKneeboard/version.h>

#include <memory>
#include <string>

#include <loader_interfaces.h>

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

  if (mSwapchain) {
    mOpenXR->xrDestroySwapchain(mSwapchain);
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
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "xrEndFrame");
  if (frameEndInfo->layerCount == 0) {
    TraceLoggingWriteStop(
      activity, "xrEndFrame", TraceLoggingValue("No layers", "Result"));
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto d3d11 = this->GetD3D11Device();
  if (!d3d11) {
    TraceLoggingWriteStop(
      activity, "xrEndFrame", TraceLoggingValue("No D3D11", "Result"));
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }

  auto snapshot = mSHM.MaybeGet(d3d11.get(), SHM::ConsumerKind::OpenXR);
  if (!snapshot.IsValid()) {
    TraceLoggingWriteStop(
      activity, "xrEndFrame", TraceLoggingValue("No snapshot", "Result"));
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

  if (mSwapchain) {
    if (!this->ConfigurationsAreCompatible(mInitialConfig, config.mVR)) {
      dprint("Incompatible swapchain due to options change, recreating");
      this->ReleaseSwapchainResources(mSwapchain);
      mOpenXR->xrDestroySwapchain(mSwapchain);
      mSwapchain = nullptr;
    }
  }

  if (!mSwapchain) {
    mInitialConfig = config.mVR;
    PixelSize size {
      TextureWidth * MaxLayers,
      TextureHeight,
    };
    mSwapchain = this->CreateSwapchain(session, size, mInitialConfig.mQuirks);
    if (!mSwapchain) {
      dprint("Failed to create swapchain");
      OPENKNEEBOARD_BREAK;
      TraceLoggingWriteStop(
        activity,
        "xrEndFrame",
        TraceLoggingValue("Failed to create swapchain", "Result"));
      return mOpenXR->xrEndFrame(session, frameEndInfo);
    }
    dprintf("Created {}x{} swapchain", size.mWidth, size.mHeight);
  }

  bool needRender = config.mVR.mQuirks.mOpenXR_AlwaysUpdateSwapchain;
  std::vector<LayerRenderInfo> layers;
  for (uint8_t i = 0; i < layerCount; ++i) {
    auto layerConfig = snapshot.GetLayerConfig(i);
    if (!layerConfig->IsValid()) {
      TraceLoggingWriteStop(
        activity,
        "xrEndFrame",
        TraceLoggingValue(i, "LayerNumber"),
        TraceLoggingValue("Invalid layer config", "Result"));
      return mOpenXR->xrEndFrame(session, frameEndInfo);
    }
    layers.push_back(LayerRenderInfo {
      .mLayerIndex = i,
      .mVR = this->GetRenderParameters(snapshot, *layerConfig, hmdPose),
    });
    needRender
      = needRender || (mRenderCacheKeys[i] != layers.back().mVR.mCacheKey);
  }

  if (needRender) {
    uint32_t swapchainTextureIndex;
    auto nextResult = mOpenXR->xrAcquireSwapchainImage(
      mSwapchain, nullptr, &swapchainTextureIndex);
    if (XR_FAILED(nextResult)) {
      dprintf("Failed to acquire swapchain image: {}", nextResult);
      OPENKNEEBOARD_BREAK;
      TraceLoggingWriteStop(
        activity,
        "xrEndFrame",
        TraceLoggingValue("Failed to acquire swapchain image", "Result"));
      return mOpenXR->xrEndFrame(session, frameEndInfo);
    }

    const scope_guard releaseSwapchainImage([this]() {
      auto nextResult = mOpenXR->xrReleaseSwapchainImage(mSwapchain, nullptr);
      if (XR_FAILED(nextResult)) {
        dprintf("Failed to release swapchain image: {}", nextResult);
        OPENKNEEBOARD_BREAK;
      }
    });

    XrSwapchainImageWaitInfo waitInfo {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
    };
    nextResult = mOpenXR->xrWaitSwapchainImage(mSwapchain, &waitInfo);
    if (XR_FAILED(nextResult)) {
      dprintf("Failed to wait for swapchain image: {}", nextResult);
      OPENKNEEBOARD_BREAK;
      TraceLoggingWriteStop(
        activity,
        "xrEndFrame",
        TraceLoggingValue("Failed to wait for swapchain image", "Result"));
      return mOpenXR->xrEndFrame(session, frameEndInfo);
    }

    for (uint8_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
      auto layer = snapshot.GetLayerConfig(layerIndex);
      auto& layerRenderInfo = layers.at(layerIndex);
      auto& renderParams = layerRenderInfo.mVR;

      if (renderParams.mIsLookingAtKneeboard) {
        topMost = layerIndex;
      }

      layerRenderInfo.mSourceRect
        = {{0, 0}, {layer->mImageWidth, layer->mImageHeight}};
      layerRenderInfo.mDestRect = {
        {layerIndex * TextureWidth, 0},
        {layer->mImageWidth, layer->mImageHeight},
      };

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
          {static_cast<int32_t>(layer->mImageWidth), static_cast<int32_t>(layer->mImageHeight)},
        },
        .imageArrayIndex = 0,
      },
      .pose = this->GetXrPosef(renderParams.mKneeboardPose),
      .size = { renderParams.mKneeboardSize.x, renderParams.mKneeboardSize.y },
    });
      nextLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(
        &kneeboardLayers.back()));
    }

    {
      winrt::com_ptr<ID3D11DeviceContext> context;
      d3d11->GetImmediateContext(context.put());
      D3D11::SavedState state(context);

      TraceLoggingThreadActivity<gTraceProvider> subActivity;
      TraceLoggingWriteStart(subActivity, "OpenXRKneeboard::RenderLayers()");
      const auto success = this->RenderLayers(
        mSwapchain,
        swapchainTextureIndex,
        snapshot,
        layers.size(),
        layers.data());
      TraceLoggingWriteStop(
        subActivity,
        "OpenXRKneeboard::RenderLayers()",
        TraceLoggingValue(success, "Result"));
      if (!success) {
        TraceLoggingWriteStop(
          activity,
          "xrEndFrame",
          TraceLoggingValue("RenderLayers failed", "Result"));
        OPENKNEEBOARD_BREAK;
        return mOpenXR->xrEndFrame(session, frameEndInfo);
      }
    }
  }

  if (topMost != layerCount - 1) {
    std::swap(kneeboardLayers.back(), kneeboardLayers.at(topMost));
  }

  XrFrameEndInfo nextFrameEndInfo {*frameEndInfo};
  nextFrameEndInfo.layers = nextLayers.data();
  nextFrameEndInfo.layerCount = static_cast<uint32_t>(nextLayers.size());

  XrResult nextResult;
  {
    TraceLoggingThreadActivity<gTraceProvider> subActivity;
    TraceLoggingWriteStart(subActivity, "next_xrEndFrame()");
    nextResult = mOpenXR->xrEndFrame(session, &nextFrameEndInfo);
    TraceLoggingWriteStop(subActivity, "next_xrEndFrame()");
  }
  if (nextResult != XR_SUCCESS) {
    OPENKNEEBOARD_BREAK;
  }
  TraceLoggingWriteStop(
    activity,
    "xrEndFrame",
    TraceLoggingValue(static_cast<const int64_t>(nextResult), "XrResult"));
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

  auto vk = findInXrNextChain<XrGraphicsBindingVulkanKHR>(
    XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, createInfo->next);
  if (vk) {
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

// Provided by XR_KHR_vulkan_enable
XrResult xrGetVulkanGraphicsRequirementsKHR(
  XrInstance instance,
  XrSystemId systemId,
  XrGraphicsRequirementsVulkanKHR* graphicsRequirements) {
  dprintf("{}()", __FUNCTION__);
  // As of 2024-01-14, the Vulkan API validation layer calls a nullptr
  // from `vkGetImageMemoryRequirements2()` if the VK API is < 1.1;
  // as there's no warnings from the layer before the crash, I wasn't
  // able to figure out if this is a bug in OpenKneeboard or the API
  // validation layer.
  //
  // As `hello_xr` is the primary testbed, uses VK 1.0, and enables the
  // debug layer in debug builds, silently upgrade to VK 1.1.
  //
  // We only *need* to this for hello_xr + debug builds, but do it always
  // so the behavior is consistent.
  //
  // hello_xr actually ignores this at the moment, so only the `Vulkan2`
  // backend works.
  auto ret = gNext->xrGetVulkanGraphicsRequirementsKHR(
    instance, systemId, graphicsRequirements);
  if (XR_FAILED(ret)) {
    dprintf("WARNING: next failed {}", ret);
    return ret;
  }

  // This uses XR versions, not the VK version constants
  constexpr auto v1_1 = XR_MAKE_VERSION(1, 1, 0);
  if (graphicsRequirements->minApiVersionSupported >= v1_1) {
    dprintf(
      "OK: Runtime is requesting a new enough VK 1.1: {}",
      graphicsRequirements->minApiVersionSupported);
    return ret;
  }

  if (graphicsRequirements->maxApiVersionSupported < v1_1) {
    dprintf(
      "WARNING: OpenXR runtime does not support VK 1.1; max is {}",
      graphicsRequirements->maxApiVersionSupported);
    return ret;
  }

  dprintf(
    "WARNING: Upgrading from VK {} to {}",
    graphicsRequirements->minApiVersionSupported,
    v1_1);
  graphicsRequirements->minApiVersionSupported = v1_1;
  return ret;
}

XrResult GetVulkanExtensions(
  uint32_t bufferCapacityInput,
  uint32_t* bufferCountOutput,
  char* buffer,
  const auto& requiredExtensions,
  const std::function<XrResult(
    uint32_t bufferCapacityInput,
    uint32_t* bufferCountOutput,
    char* buffer)>& next) {
  auto ret = next(0, bufferCountOutput, nullptr);
  if (XR_FAILED(ret)) {
    return ret;
  }

  // Space-separated list of extensions
  uint32_t& count = *bufferCountOutput;
  std::string extensions;
  extensions.resize(count);

  ret = next(count, &count, extensions.data());
  if (XR_FAILED(ret)) {
    return ret;
  }

  // Remove trailing null
  extensions.resize(count - 1);
  dprintf("Runtime requested extensions: {}", extensions);

  for (const auto& ext: requiredExtensions) {
    const std::string_view view {ext};
    size_t offset = 0;
    bool found = false;
    while ((offset + view.size()) < extensions.size()) {
      auto it = extensions.find(ext, offset);
      if (it == std::string::npos) {
        break;
      }

      if (it + view.size() == extensions.size()) {
        // Last one in the list
        found = true;
        break;
      }

      if (extensions.at(it + view.size()) == ' ') {
        // In the list
        found = true;
        break;
      }

      // If we got here, another extension starts with this extension name
      offset += view.size();
    }

    if (found) {
      // Next extension
      continue;
    }
    if (extensions.empty()) {
      extensions = std::string_view {ext};
    } else {
      extensions += std::format(" {}", view);
    }
  }
  dprintf("Requesting extensions: {}", extensions);

  *bufferCountOutput = extensions.size() + 1;
  if (bufferCapacityInput == 0 || (!buffer)) {
    return ret;
  }

  if (buffer && (bufferCapacityInput >= *bufferCountOutput)) {
    memcpy_s(
      buffer, bufferCapacityInput, extensions.c_str(), extensions.size() + 1);
    return ret;
  }

  return XR_ERROR_SIZE_INSUFFICIENT;
}

// Provided by XR_KHR_vulkan_enable
XrResult xrGetVulkanInstanceExtensionsKHR(
  XrInstance instance,
  XrSystemId systemId,
  uint32_t bufferCapacityInput,
  uint32_t* bufferCountOutput,
  char* buffer) {
  dprintf("{}()", __FUNCTION__);

  return GetVulkanExtensions(
    bufferCapacityInput,
    bufferCountOutput,
    buffer,
    OpenXRVulkanKneeboard::VK_INSTANCE_EXTENSIONS,
    [&](uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
      -> XrResult {
      return gNext->xrGetVulkanInstanceExtensionsKHR(
        instance, systemId, bufferCapacityInput, bufferCountOutput, buffer);
    });
}

// Provided by XR_KHR_vulkan_enable
XrResult xrGetVulkanDeviceExtensionsKHR(
  XrInstance instance,
  XrSystemId systemId,
  uint32_t bufferCapacityInput,
  uint32_t* bufferCountOutput,
  char* buffer) {
  dprintf("{}()", __FUNCTION__);

  return GetVulkanExtensions(
    bufferCapacityInput,
    bufferCountOutput,
    buffer,
    OpenXRVulkanKneeboard::VK_DEVICE_EXTENSIONS,
    [&](uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
      -> XrResult {
      return gNext->xrGetVulkanDeviceExtensionsKHR(
        instance, systemId, bufferCapacityInput, bufferCountOutput, buffer);
    });
}

template <class T>
XrResult CreateWithVKExtensions(
  const T* origCreateInfo,
  const auto& requiredExtensions,
  const std::function<XrResult(const T*)>& createFunc) {
  auto createInfo = *origCreateInfo;
  auto vci = *createInfo.vulkanCreateInfo;
  createInfo.vulkanCreateInfo = &vci;

  std::vector<const char*> extensions;
  for (size_t i = 0; i < vci.enabledExtensionCount; ++i) {
    extensions.push_back(vci.ppEnabledExtensionNames[i]);
  }

  for (const auto& ext: requiredExtensions) {
    const auto view = std::string_view {ext};
    auto it = std::ranges::find(extensions, view);
    if (it == extensions.end()) {
      extensions.push_back(ext);
    }
  }
  vci.enabledExtensionCount = extensions.size();
  vci.ppEnabledExtensionNames = extensions.data();

  dprint("Enabled VK extensions:");
  for (size_t i = 0; i < vci.enabledExtensionCount; ++i) {
    dprintf("- {}", vci.ppEnabledExtensionNames[i]);
  }

  return createFunc(&createInfo);
}

// Provided by XR_KHR_vulkan_enable2
XrResult xrCreateVulkanInstanceKHR(
  XrInstance instance,
  const XrVulkanInstanceCreateInfoKHR* origCreateInfo,
  VkInstance* vulkanInstance,
  VkResult* vulkanResult) {
  dprintf("{}()", __FUNCTION__);

  // As of 2024-01-14, the Vulkan API validation layer calls a nullptr
  // from `vkGetImageMemoryRequirements2()` if the VK API is < 1.1,
  // and from `vkImportSemaphoreWin32HandleKHR()` if the VK API is < 1.3.
  //
  // As there's no warnings from the layer before the crash, I wasn't
  // able to figure out if this is a bug in OpenKneeboard or the API
  // validation layer.
  //
  // As `hello_xr` is the primary testbed, uses VK 1.0, and enables the
  // debug layer in debug builds, silently upgrade to VK 1.3
  //
  // We only *need* to this for hello_xr + debug builds, but do it always
  // so the behavior is consistent.
  auto createInfo = *origCreateInfo;

  auto vci = *createInfo.vulkanCreateInfo;
  createInfo.vulkanCreateInfo = &vci;
  auto vaci = *vci.pApplicationInfo;
  vci.pApplicationInfo = &vaci;
  const auto requiredVKApiVersion = VK_API_VERSION_1_3;

  if (vaci.apiVersion >= requiredVKApiVersion) {
    dprintf("App is requesting VK version {}", vaci.apiVersion);
  } else {
    vaci.apiVersion = requiredVKApiVersion;
    dprintf(
      "WARNING: upgrading app from VK {} to {}",
      origCreateInfo->vulkanCreateInfo->pApplicationInfo->apiVersion,
      vaci.apiVersion);
  }

  const auto ret = CreateWithVKExtensions<XrVulkanInstanceCreateInfoKHR>(
    &createInfo,
    OpenXRVulkanKneeboard::VK_INSTANCE_EXTENSIONS,
    [&](const auto* createInfo) {
      return gNext->xrCreateVulkanInstanceKHR(
        instance, createInfo, vulkanInstance, vulkanResult);
    });
  if (XR_FAILED(ret)) {
    return ret;
  }

  if (createInfo.pfnGetInstanceProcAddr) {
    gPFN_vkGetInstanceProcAddr = createInfo.pfnGetInstanceProcAddr;
  }
  if (createInfo.vulkanAllocator) {
    gVKAllocator = createInfo.vulkanAllocator;
  }

  return ret;
}

// Provided by XR_KHR_vulkan_enable2
XrResult xrCreateVulkanDeviceKHR(
  XrInstance instance,
  const XrVulkanDeviceCreateInfoKHR* origCreateInfo,
  VkDevice* vulkanDevice,
  VkResult* vulkanResult) {
  dprintf("{}()", __FUNCTION__);

  auto createInfo = *origCreateInfo;

  auto vci = *createInfo.vulkanCreateInfo;
  createInfo.vulkanCreateInfo = &vci;

  VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
    .pNext = const_cast<void*>(vci.pNext),
    .timelineSemaphore = VK_TRUE,
  };
  vci.pNext = &timelineFeatures;

  const auto ret = CreateWithVKExtensions<XrVulkanDeviceCreateInfoKHR>(
    &createInfo,
    OpenXRVulkanKneeboard::VK_DEVICE_EXTENSIONS,
    [&](const auto* createInfo) {
      return gNext->xrCreateVulkanDeviceKHR(
        instance, createInfo, vulkanDevice, vulkanResult);
    });
  if (XR_FAILED(ret)) {
    return ret;
  }

  if (createInfo.pfnGetInstanceProcAddr) {
    gPFN_vkGetInstanceProcAddr = createInfo.pfnGetInstanceProcAddr;
  }
  if (createInfo.vulkanAllocator) {
    gVKAllocator = createInfo.vulkanAllocator;
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

  ///// START XR_KHR_vulkan_enable /////
  if (name == "xrGetVulkanDeviceExtensionsKHR") {
    *function
      = reinterpret_cast<PFN_xrVoidFunction>(&xrGetVulkanDeviceExtensionsKHR);
    return XR_SUCCESS;
  }
  if (name == "xrGetVulkanInstanceExtensionsKHR") {
    *function
      = reinterpret_cast<PFN_xrVoidFunction>(&xrGetVulkanInstanceExtensionsKHR);
    return XR_SUCCESS;
  }
  if (name == "xrGetVulkanGraphicsRequirementsKHR") {
    *function = reinterpret_cast<PFN_xrVoidFunction>(
      &xrGetVulkanGraphicsRequirementsKHR);
    return XR_SUCCESS;
  }
  ///// END XR_KHR_vulkan_enable /////

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
