// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRKneeboard.hpp"

#include "OpenXRD3D11Kneeboard.hpp"
#include "OpenXRD3D12Kneeboard.hpp"
#include "OpenXRGestureTracker.hpp"
#include "OpenXRNext.hpp"
#include "OpenXRVulkanKneeboard.hpp"

#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/SpriteAtlas.hpp>
#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <loader_interfaces.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <shims/openxr/openxr.h>

#include <wil/resource.h>

#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>

namespace OpenKneeboard {

auto& Next() {
  static std::shared_ptr<OpenXRNext> it;
  return it;
}

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
bool gHaveXR_EXT_hand_tracking_data_source {false};
template <class T>
uint64_t XrHandleKey(const T& handle) noexcept {
#ifdef OPENKNEEBOARD_32BIT_BUILD
  return handle.mRawValue;
#else
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
#endif
}

template <class T>
bool SameXrHandle(const T& a, const T& b) noexcept {
  return XrHandleKey(a) == XrHandleKey(b);
}

XrInstance gXRInstance {XR_NULL_HANDLE};
XrPath gLeftHandPath {XR_NULL_PATH};
XrPath gRightHandPath {XR_NULL_PATH};

XrActionSet gManipActionSet {XR_NULL_HANDLE};
XrAction gManipAimPoseAction {XR_NULL_HANDLE};
XrAction gManipGripPoseAction {XR_NULL_HANDLE};
XrAction gManipGripFloatAction {XR_NULL_HANDLE};
XrAction gManipGripBoolAction {XR_NULL_HANDLE};
XrAction gManipTriggerFloatAction {XR_NULL_HANDLE};
XrAction gManipSelectBoolAction {XR_NULL_HANDLE};
XrAction gManipStickVec2Action {XR_NULL_HANDLE};
XrAction gManipTrackpadVec2Action {XR_NULL_HANDLE};
bool gAnySessionAttached {false};

enum class ManipActionKind : uint8_t {
  None = 0,
  Grip = 1,
  Trigger = 2,
  Thumbstick = 3,
};

struct PoseActionInfo {
  bool mHasLeft {false};
  bool mHasRight {false};
  int mScore {0};
  int mAimScore {0};
};

struct ManipActionInfo {
  ManipActionKind mKind {ManipActionKind::None};
  bool mHasLeft {false};
  bool mHasRight {false};
};

struct ControllerActionCache {
  bool mGripHeld {false};
  bool mTriggerHeld {false};
  float mThumbstickY {0.0f};
  std::chrono::steady_clock::time_point mLastActive {};
};

struct AppHandSpace {
  XrSession mSession {XR_NULL_HANDLE};
  XrSpace mSpace {XR_NULL_HANDLE};
  int mScore {-1};
};

std::mutex gHandSpaceMutex;
std::unordered_map<uint64_t, PoseActionInfo> gPoseActions;
std::unordered_map<uint64_t, ManipActionInfo> gManipActions;
std::array<AppHandSpace, 2> gAppHandSpaces {};
std::array<AppHandSpace, 2> gAppAimSpaces {};
std::array<ControllerActionCache, 2> gControllerStates {};
std::unordered_set<XrPath> gGameSuggestedProfiles;
std::unordered_set<uint64_t> gAttachedSessions;
std::unordered_set<uint64_t> gManipAttachedSessions;
std::unordered_map<uint64_t, bool> gSyncedManipThisFrame;

bool ContainsCaseInsensitive(
  std::string_view haystack,
  std::string_view needle) {
  if (needle.empty() || haystack.size() < needle.size()) {
    return false;
  }
  const auto it = std::search(
    haystack.begin(),
    haystack.end(),
    needle.begin(),
    needle.end(),
    [](char a, char b) {
      return std::tolower(static_cast<unsigned char>(a))
        == std::tolower(static_cast<unsigned char>(b));
    });
  return it != haystack.end();
}

void DestroyManipActions() {
  if (gManipActionSet && Next() && Next()->xrDestroyActionSet) {
    Next()->xrDestroyActionSet(gManipActionSet);
  }
  gManipActionSet = XR_NULL_HANDLE;
  gManipAimPoseAction = XR_NULL_HANDLE;
  gManipGripPoseAction = XR_NULL_HANDLE;
  gManipGripFloatAction = XR_NULL_HANDLE;
  gManipGripBoolAction = XR_NULL_HANDLE;
  gManipTriggerFloatAction = XR_NULL_HANDLE;
  gManipSelectBoolAction = XR_NULL_HANDLE;
  gManipStickVec2Action = XR_NULL_HANDLE;
  gManipTrackpadVec2Action = XR_NULL_HANDLE;
  gAnySessionAttached = false;
  gGameSuggestedProfiles.clear();
  gAttachedSessions.clear();
  gManipAttachedSessions.clear();
  gSyncedManipThisFrame.clear();
}

void InitManipActions(XrInstance instance) {
  if (
    !Next() || !Next()->xrCreateActionSet || !Next()->xrCreateAction
    || !Next()->xrStringToPath) {
    return;
  }
  if (gLeftHandPath == XR_NULL_PATH || gRightHandPath == XR_NULL_PATH) {
    return;
  }
  XrActionSetCreateInfo setInfo {
    .type = XR_TYPE_ACTION_SET_CREATE_INFO,
    .next = nullptr,
    .priority = 0,
  };
  strncpy_s(setInfo.actionSetName, "openkneeboard_manip", _TRUNCATE);
  strncpy_s(
    setInfo.localizedActionSetName, "OpenKneeboard Manipulation", _TRUNCATE);
  if (XR_FAILED(
        Next()->xrCreateActionSet(instance, &setInfo, &gManipActionSet))) {
    gManipActionSet = XR_NULL_HANDLE;
    return;
  }

  const std::array<XrPath, 2> handPaths {gLeftHandPath, gRightHandPath};
  const auto createAct = [&](
                           const char* name,
                           const char* locName,
                           XrActionType type,
                           XrAction& out) -> bool {
    XrActionCreateInfo ci {
      .type = XR_TYPE_ACTION_CREATE_INFO,
      .next = nullptr,
      .actionType = type,
      .countSubactionPaths = static_cast<uint32_t>(handPaths.size()),
      .subactionPaths = handPaths.data(),
    };
    strncpy_s(ci.actionName, name, _TRUNCATE);
    strncpy_s(ci.localizedActionName, locName, _TRUNCATE);
    return XR_SUCCEEDED(Next()->xrCreateAction(gManipActionSet, &ci, &out));
  };

  const bool ok =
    createAct(
      "okb_aim_pose",
      "OpenKneeboard Aim Pose",
      XR_ACTION_TYPE_POSE_INPUT,
      gManipAimPoseAction)
    && createAct(
      "okb_grip_pose",
      "OpenKneeboard Grip Pose",
      XR_ACTION_TYPE_POSE_INPUT,
      gManipGripPoseAction)
    && createAct(
      "okb_grip_value",
      "OpenKneeboard Grip Value",
      XR_ACTION_TYPE_FLOAT_INPUT,
      gManipGripFloatAction)
    && createAct(
      "okb_grip_click",
      "OpenKneeboard Grip Click",
      XR_ACTION_TYPE_BOOLEAN_INPUT,
      gManipGripBoolAction)
    && createAct(
      "okb_trigger_value",
      "OpenKneeboard Trigger Value",
      XR_ACTION_TYPE_FLOAT_INPUT,
      gManipTriggerFloatAction)
    && createAct(
      "okb_select_click",
      "OpenKneeboard Select Click",
      XR_ACTION_TYPE_BOOLEAN_INPUT,
      gManipSelectBoolAction)
    && createAct(
      "okb_thumbstick",
      "OpenKneeboard Thumbstick",
      XR_ACTION_TYPE_VECTOR2F_INPUT,
      gManipStickVec2Action)
    && createAct(
      "okb_trackpad",
      "OpenKneeboard Trackpad",
      XR_ACTION_TYPE_VECTOR2F_INPUT,
      gManipTrackpadVec2Action);
  if (!ok) {
    dprint.Warning("Failed creating OpenKneeboard manipulation actions");
    DestroyManipActions();
  } else {
    dprint("Created OpenKneeboard OpenXR manipulation ActionSet");
  }
}

std::vector<XrActionSuggestedBinding> GetManipBindingsForProfileStr(
  XrInstance instance,
  std::string_view profileName) {
  if (!gManipActionSet || !Next() || !Next()->xrStringToPath) {
    return {};
  }
  std::vector<XrActionSuggestedBinding> out;
  const auto addBinding = [&](XrAction act, const char* pathStr) {
    if (!act) {
      return;
    }
    XrPath p {XR_NULL_PATH};
    if (
      XR_SUCCEEDED(Next()->xrStringToPath(instance, pathStr, &p))
      && p != XR_NULL_PATH) {
      out.push_back({act, p});
    }
  };

  if (
    profileName == "/interaction_profiles/oculus/touch_controller"
    || profileName == "/interaction_profiles/facebook/touch_controller_pro"
    || profileName == "/interaction_profiles/meta/touch_controller_plus"
    || profileName == "/interaction_profiles/valve/index_controller"
    || profileName == "/interaction_profiles/hp/mixed_reality_controller"
    || profileName == "/interaction_profiles/bytedance/pico_neo3_controller"
    || profileName == "/interaction_profiles/bytedance/pico4_controller") {
    addBinding(gManipAimPoseAction, "/user/hand/left/input/aim/pose");
    addBinding(gManipAimPoseAction, "/user/hand/right/input/aim/pose");
    addBinding(gManipGripPoseAction, "/user/hand/left/input/grip/pose");
    addBinding(gManipGripPoseAction, "/user/hand/right/input/grip/pose");
    addBinding(gManipGripFloatAction, "/user/hand/left/input/squeeze/value");
    addBinding(gManipGripFloatAction, "/user/hand/right/input/squeeze/value");
    addBinding(gManipTriggerFloatAction, "/user/hand/left/input/trigger/value");
    addBinding(
      gManipTriggerFloatAction, "/user/hand/right/input/trigger/value");
    addBinding(gManipStickVec2Action, "/user/hand/left/input/thumbstick");
    addBinding(gManipStickVec2Action, "/user/hand/right/input/thumbstick");
  } else if (
    profileName == "/interaction_profiles/microsoft/motion_controller"
    || profileName == "/interaction_profiles/htc/vive_cosmos_controller") {
    addBinding(gManipAimPoseAction, "/user/hand/left/input/aim/pose");
    addBinding(gManipAimPoseAction, "/user/hand/right/input/aim/pose");
    addBinding(gManipGripPoseAction, "/user/hand/left/input/grip/pose");
    addBinding(gManipGripPoseAction, "/user/hand/right/input/grip/pose");
    addBinding(gManipGripBoolAction, "/user/hand/left/input/squeeze/click");
    addBinding(gManipGripBoolAction, "/user/hand/right/input/squeeze/click");
    addBinding(gManipTriggerFloatAction, "/user/hand/left/input/trigger/value");
    addBinding(
      gManipTriggerFloatAction, "/user/hand/right/input/trigger/value");
    addBinding(gManipStickVec2Action, "/user/hand/left/input/thumbstick");
    addBinding(gManipStickVec2Action, "/user/hand/right/input/thumbstick");
  } else if (profileName == "/interaction_profiles/htc/vive_controller") {
    addBinding(gManipAimPoseAction, "/user/hand/left/input/aim/pose");
    addBinding(gManipAimPoseAction, "/user/hand/right/input/aim/pose");
    addBinding(gManipGripPoseAction, "/user/hand/left/input/grip/pose");
    addBinding(gManipGripPoseAction, "/user/hand/right/input/grip/pose");
    addBinding(gManipGripBoolAction, "/user/hand/left/input/squeeze/click");
    addBinding(gManipGripBoolAction, "/user/hand/right/input/squeeze/click");
    addBinding(gManipTriggerFloatAction, "/user/hand/left/input/trigger/value");
    addBinding(
      gManipTriggerFloatAction, "/user/hand/right/input/trigger/value");
    addBinding(gManipTrackpadVec2Action, "/user/hand/left/input/trackpad");
    addBinding(gManipTrackpadVec2Action, "/user/hand/right/input/trackpad");
  } else if (profileName == "/interaction_profiles/khr/simple_controller") {
    addBinding(gManipAimPoseAction, "/user/hand/left/input/aim/pose");
    addBinding(gManipAimPoseAction, "/user/hand/right/input/aim/pose");
    addBinding(gManipGripPoseAction, "/user/hand/left/input/grip/pose");
    addBinding(gManipGripPoseAction, "/user/hand/right/input/grip/pose");
    addBinding(gManipSelectBoolAction, "/user/hand/left/input/select/click");
    addBinding(gManipSelectBoolAction, "/user/hand/right/input/select/click");
  }
  return out;
}

constexpr std::array<const char*, 5> kCoreProfiles {
  "/interaction_profiles/oculus/touch_controller",
  "/interaction_profiles/valve/index_controller",
  "/interaction_profiles/microsoft/motion_controller",
  "/interaction_profiles/htc/vive_controller",
  "/interaction_profiles/khr/simple_controller",
};

std::vector<XrActionSuggestedBinding> GetManipBindingsForProfilePath(
  XrInstance instance,
  XrPath profile) {
  if (!Next()) {
    return {};
  }
  if (Next()->xrPathToString && profile != XR_NULL_PATH) {
    char buf[XR_MAX_PATH_LENGTH] {};
    uint32_t outLen = 0;
    if (
      XR_SUCCEEDED(Next()->xrPathToString(
        instance, profile, static_cast<uint32_t>(sizeof(buf)), &outLen, buf))
      && outLen > 1) {
      const auto bindings = GetManipBindingsForProfileStr(
        instance, std::string_view {buf, outLen - 1});
      if (!bindings.empty()) {
        return bindings;
      }
    }
  }
  if (!Next()->xrStringToPath) {
    return {};
  }
  for (const char* name : kCoreProfiles) {
    XrPath p {XR_NULL_PATH};
    if (
      XR_SUCCEEDED(Next()->xrStringToPath(instance, name, &p)) && p == profile) {
      return GetManipBindingsForProfileStr(instance, name);
    }
  }
  return {};
}

void SuggestMissingManipBindings(XrInstance instance) {
  if (
    !instance || !gManipActionSet || !Next() || !Next()->xrStringToPath
    || !Next()->xrSuggestInteractionProfileBindings) {
    return;
  }
  bool alreadyAttached = false;
  std::unordered_set<XrPath> suggestedCopy;
  {
    std::scoped_lock lock(gHandSpaceMutex);
    alreadyAttached = gAnySessionAttached;
    suggestedCopy = gGameSuggestedProfiles;
  }
  if (alreadyAttached) {
    return;
  }
  for (const char* profileName : kCoreProfiles) {
    XrPath profilePath {XR_NULL_PATH};
    if (
      XR_FAILED(Next()->xrStringToPath(instance, profileName, &profilePath))
      || profilePath == XR_NULL_PATH) {
      continue;
    }
    if (suggestedCopy.contains(profilePath)) {
      continue;
    }
    const auto bindings = GetManipBindingsForProfileStr(instance, profileName);
    if (bindings.empty()) {
      continue;
    }
    XrInteractionProfileSuggestedBinding info {
      .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
      .next = nullptr,
      .interactionProfile = profilePath,
      .countSuggestedBindings = static_cast<uint32_t>(bindings.size()),
      .suggestedBindings = bindings.data(),
    };
    const auto res =
      Next()->xrSuggestInteractionProfileBindings(instance, &info);
    if (XR_FAILED(res)) {
      dprint.Warning(
        "xrSuggestInteractionProfileBindings({}) returned {}",
        profileName,
        static_cast<int>(res));
    }
  }
}
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

static OpenXRRuntimeID gRuntime {};
static PFN_vkGetInstanceProcAddr gPFN_vkGetInstanceProcAddr {nullptr};
static const VkAllocationCallbacks* gVKAllocator {nullptr};

static HMODULE LibVulkan() {
  static wil::unique_hmodule data {LoadLibraryW(L"vulkan-1.dll")};
  return data.get();
}

// xrResultAsString exists, but isn't reliably giving useful results, e.g.
// it fails on the Meta XR Simulator.
static std::string xrresult_to_string(const XrResult code) {
  // xrResultAsString exists, but isn't reliably giving useful results, e.g.
  // it fails on the Meta XR Simulator.
  switch (code) {
#define XR_RESULT_CASE(enum_name, value) \
  case enum_name: \
    return std::format("{} ({})", #enum_name, static_cast<int>(code));
    XR_LIST_ENUM_XrResult(XR_RESULT_CASE);
#undef XR_RESULT_CASE
    default:
      return std::to_string(static_cast<int>(code));
  }
}

static inline XrResult check_xrresult(
  XrResult code,
  const std::source_location& loc = std::source_location::current()) {
  if (XR_SUCCEEDED(code)) [[likely]] {
    return code;
  }

  const auto codeAsString = xrresult_to_string(code);
  fatal(loc, "OpenXR call failed: {}", codeAsString);
}

OpenXRKneeboard::OpenXRKneeboard(
  XrInstance instance,
  XrSystemId system,
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next)
  : mOpenXR(next), mXRInstance(instance), mSession(session) {
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
  mGestureTracker = std::make_unique<OpenXRGestureTracker>(
    instance, system, session, next, gHaveXR_EXT_hand_tracking_data_source);

  if (gManipActionSet && oxr->xrCreateActionSpace) {
    const std::array<XrPath, 2> handPaths {gLeftHandPath, gRightHandPath};
    for (size_t i = 0; i < 2; ++i) {
      if (handPaths[i] == XR_NULL_PATH) {
        continue;
      }
      if (gManipAimPoseAction) {
        XrActionSpaceCreateInfo aimInfo {
          .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
          .next = nullptr,
          .action = gManipAimPoseAction,
          .subactionPath = handPaths[i],
          .poseInActionSpace = XR_POSEF_IDENTITY,
        };
        if (XR_FAILED(oxr->xrCreateActionSpace(
              session, &aimInfo, &mOwnAimSpaces[i]))) {
          mOwnAimSpaces[i] = XR_NULL_HANDLE;
        }
      }
      if (gManipGripPoseAction) {
        XrActionSpaceCreateInfo gripInfo {
          .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
          .next = nullptr,
          .action = gManipGripPoseAction,
          .subactionPath = handPaths[i],
          .poseInActionSpace = XR_POSEF_IDENTITY,
        };
        if (XR_FAILED(oxr->xrCreateActionSpace(
              session, &gripInfo, &mOwnGripSpaces[i]))) {
          mOwnGripSpaces[i] = XR_NULL_HANDLE;
        }
      }
    }
  }
}

OpenXRKneeboard::~OpenXRKneeboard() {
  OPENKNEEBOARD_TraceLoggingScope("OpenXRKneeboard::~OpenXRKneeboard()");
  for (auto& space : mOwnAimSpaces) {
    if (space) {
      mOpenXR->xrDestroySpace(space);
      space = XR_NULL_HANDLE;
    }
  }
  for (auto& space : mOwnGripSpaces) {
    if (space) {
      mOpenXR->xrDestroySpace(space);
      space = XR_NULL_HANDLE;
    }
  }
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

  const auto& destSprites = this->GetSpriteAtlas(*frame);
  const auto swapchainDimensions = destSprites.GetAtlasSize();

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

  ++mEndFrameCount;
  this->EnsureManipActionSetAttached();

  bool manipAttached = false;
  bool alreadySyncedThisFrame = false;
  {
    std::scoped_lock lock(gHandSpaceMutex);
    manipAttached = gManipAttachedSessions.contains(XrHandleKey(mSession));
    auto& synced = gSyncedManipThisFrame[XrHandleKey(mSession)];
    alreadySyncedThisFrame = synced;
    synced = false;
  }
  if (manipAttached && !alreadySyncedThisFrame && mOpenXR->xrSyncActions) {
    XrActiveActionSet activeSet {
      .actionSet = gManipActionSet,
      .subactionPath = XR_NULL_PATH,
    };
    XrActionsSyncInfo syncInfo {
      .type = XR_TYPE_ACTIONS_SYNC_INFO,
      .next = nullptr,
      .countActiveActionSets = 1,
      .activeActionSets = &activeSet,
    };
    mOpenXR->xrSyncActions(mSession, &syncInfo);
  }

  const auto now = std::chrono::steady_clock::now();
  const float dt = mLastFrameTime
    ? std::chrono::duration<float>(now - *mLastFrameTime).count()
    : 0.016f;
  mLastFrameTime = now;
  const auto effectiveVR = this->GetEffectiveVRSettings(frame->mConfig.mVR);
  const auto effectiveMode = GetEffectiveFollowMode(effectiveVR.mHeadFollow);
  SHM::Config effectiveConfig = frame->mConfig;
  effectiveConfig.mVR = effectiveVR;

  const auto hmdPose = this->GetHMDPose(frameEndInfo->displayTime);
  std::optional<VRKneeboard::Pose> handMountPose;
  if (mGestureTracker) {
    mGestureTracker->LocateHands(mLocalSpace, frameEndInfo->displayTime);
    handMountPose = mGestureTracker->GetHandMountPose(effectiveMode);
  }
  if (!handMountPose) {
    handMountPose =
      this->LocateAppHandMountPose(effectiveMode, frameEndInfo->displayTime);
  }

  const uint8_t preferredHand = effectiveVR.mHeadFollow.mPinnedHand == 1 ? 0 : 1;
  auto controllerInputs =
    this->BuildControllerInputs(preferredHand, frameEndInfo->displayTime);
  if (
    !controllerInputs[0].mValid && !controllerInputs[1].mValid
    && effectiveVR.mEditModeEnabled && mGestureTracker) {
    if (const auto handRay = mGestureTracker->GetHandRayInput(preferredHand)) {
      controllerInputs[0] = *handRay;
    }
  }

  this->UpdateControllerManipulation(
    effectiveConfig,
    frame->mLayers,
    hmdPose,
    handMountPose,
    controllerInputs,
    dt);
  effectiveConfig.mVR = this->GetEffectiveVRSettings(frame->mConfig.mVR);
  auto vrLayers = this->GetLayers(
    effectiveConfig, frame->mLayers, hmdPose, dt, handMountPose);
  if (mGestureTracker) {
    std::optional<VRKneeboard::Layer> activeLayer;
    for (const auto& l : vrLayers) {
      if (l.mLayerConfig->mLayerID == frame->mConfig.mGlobalInputLayerID) {
        activeLayer = l;
        break;
      }
    }
    mGestureTracker->PollLocatedGestures(
      hmdPose, activeLayer, frame->mConfig.mVR.mGestures);
  }
  const auto layerCount =
    (vrLayers.size() + frameEndInfo->layerCount) <= mMaxLayerCount
    ? vrLayers.size()
    : (mMaxLayerCount - frameEndInfo->layerCount);
  if (layerCount == 0) {
    TraceLoggingWriteTagged(activity, "No active layers");
    return mOpenXR->xrEndFrame(session, frameEndInfo);
  }
  vrLayers.resize(layerCount);

  mNextCompositionLayers.clear();
  mNextCompositionLayers.reserve(frameEndInfo->layerCount + layerCount + 1);
  std::copy(
    frameEndInfo->layers,
    &frameEndInfo->layers[frameEndInfo->layerCount],
    std::back_inserter(mNextCompositionLayers));

  uint8_t topMost = static_cast<uint8_t>(layerCount - 1);
  inplace_vector<SHM::LayerSprite, MaxViewCount> layerSprites;
  inplace_vector<XrCompositionLayerQuad, MaxViewCount> addedXRLayers;
  const auto& visuals = this->GetManipulationVisuals();
  std::optional<PixelRect> targetDestRect;

  for (size_t layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    const auto [layer, params] = vrLayers.at(layerIndex);
    OPENKNEEBOARD_TraceLoggingScopedActivity(
      layerActivity,
      "OpenXRKneeboard::xrEndFrame()/LayerSprites",
      TraceLoggingValue(layerIndex, "LayerIndex"),
      TraceLoggingHexUInt64(layer->mLayerID, "LayerID"),
      OPENKNEEBOARD_TraceLoggingRect(
        layer->mVR.mLocationOnTexture, "LocationOnTexture"));
    const auto destRect = destSprites.GetRect(static_cast<uint8_t>(layerIndex));
    const auto& sprite = layerSprites.push_back(
      SHM::LayerSprite {
        .mSourceRect = layer->mVR.mLocationOnTexture,
        .mDestRect = destRect,
        .mOpacity = params.mKneeboardOpacity,
      });
    TraceLoggingWriteTagged(
      layerActivity,
      "OpenXRKneeboard::xrEndFrame()/LayerSprites/Sprite",
      OPENKNEEBOARD_TraceLoggingRect(sprite.mSourceRect, "SourceRect"),
      OPENKNEEBOARD_TraceLoggingRect(sprite.mDestRect, "DestRect"),
      TraceLoggingValue(sprite.mOpacity, "Opacity"));
    if (layer->mLayerID == frame->mConfig.mGlobalInputLayerID) {
      topMost = static_cast<uint8_t>(layerIndex);
    }
    if (
      (visuals.mTargetLayerID != 0 && layer->mLayerID == visuals.mTargetLayerID)
      || (visuals.mTargetLayerID == 0
          && (!targetDestRect
              || layer->mLayerID == frame->mConfig.mGlobalInputLayerID))) {
      targetDestRect = destRect;
    }

    static_assert(
      SHM::SHARED_TEXTURE_IS_PREMULTIPLIED,
      "Use premultiplied alpha in shared texture, or pass "
      "XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT");
    const auto updatedMode =
      GetEffectiveFollowMode(effectiveConfig.mVR.mHeadFollow);
    const bool useDirectViewSpace =
      !this->IsManipulationDragging()
      && updatedMode == VRFollowMode::HeadLocked
      && !effectiveConfig.mVR.mHeadFollow.mLockRoll
      && !effectiveConfig.mVR.mHeadFollow.mLockPitch;
    addedXRLayers.push_back({
      .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
      .next = nullptr,
      .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
      .space = useDirectViewSpace ? mViewSpace : mLocalSpace,
      .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
      .subImage =
        XrSwapchainSubImage {
          .swapchain = mSwapchain,
          .imageRect = destRect.StaticCast<int, XrRect2Di>(),
          .imageArrayIndex = 0,
        },
      .pose = useDirectViewSpace
        ? this->GetViewRelativeXrPosef(effectiveConfig.mVR.mHeadFollow)
        : this->GetXrPosef(params.mKneeboardPose),
      .size = {params.mKneeboardSize.x, params.mKneeboardSize.y},
    });
  }

  if (topMost != layerCount - 1) {
    std::swap(addedXRLayers.back(), addedXRLayers.at(topMost));
  }
  for (size_t i = 0; i < addedXRLayers.size(); ++i) {
    mNextCompositionLayers.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(&addedXRLayers[i]));
  }

  const bool hasGizmoStrip =
    frame->mConfig.mTextureSize.mWidth >= 128
    && frame->mConfig.mTextureSize.mHeight > 32;

  if (hasGizmoStrip) {
    const uint32_t gizmoY = frame->mConfig.mTextureSize.mHeight - 32u;
    const PixelRect gizmoBeamSrc {{0u, gizmoY}, {32u, 32u}};
    const PixelRect gizmoReticleSrc {{32u, gizmoY}, {32u, 32u}};
    const PixelRect gizmoAmberSrc {{64u, gizmoY}, {32u, 32u}};
    const PixelRect gizmoGoldSrc {{96u, gizmoY}, {32u, 32u}};

    if ((visuals.mEditModeActive || visuals.mContentPointerActive) && targetDestRect) {
      const PixelRect& dr = *targetDestRect;
      const uint32_t minDim =
        (std::min)(dr.mSize.mWidth, dr.mSize.mHeight);
      const uint32_t borderT =
        std::clamp(minDim / 110u, 3u, 8u);

      if (
        visuals.mEditModeActive
        && !effectiveConfig.mVR.mEditModeEnabled
        && (layerSprites.size() + 4) <= MaxViewCount) {
        layerSprites.push_back({
          .mSourceRect = gizmoAmberSrc,
          .mDestRect = {dr.mOffset, {dr.mSize.mWidth, borderT}},
          .mOpacity = 1.0f,
        });
        layerSprites.push_back({
          .mSourceRect = gizmoAmberSrc,
          .mDestRect = {{dr.Left(), dr.Bottom() - borderT}, {dr.mSize.mWidth, borderT}},
          .mOpacity = 1.0f,
        });
        layerSprites.push_back({
          .mSourceRect = gizmoAmberSrc,
          .mDestRect = {dr.mOffset, {borderT, dr.mSize.mHeight}},
          .mOpacity = 1.0f,
        });
        layerSprites.push_back({
          .mSourceRect = gizmoAmberSrc,
          .mDestRect = {{dr.Right() - borderT, dr.Top()}, {borderT, dr.mSize.mHeight}},
          .mOpacity = 1.0f,
        });
      }

      if (
        visuals.mEditModeActive
        && visuals.mHoveredCorner != EditCorner::None
        && layerSprites.size() < MaxViewCount) {
        const uint32_t cSize = std::clamp(minDim / 20u, 18u, 38u);
        uint32_t cx = dr.Left();
        uint32_t cy = dr.Top();
        if (
          visuals.mHoveredCorner == EditCorner::NE
          || visuals.mHoveredCorner == EditCorner::SE) {
          cx = dr.Right() > cSize ? (dr.Right() - cSize) : dr.Left();
        }
        if (
          visuals.mHoveredCorner == EditCorner::SW
          || visuals.mHoveredCorner == EditCorner::SE) {
          cy = dr.Bottom() > cSize ? (dr.Bottom() - cSize) : dr.Top();
        }
        layerSprites.push_back({
          .mSourceRect = gizmoGoldSrc,
          .mDestRect = {{cx, cy}, {cSize, cSize}},
          .mOpacity = 1.0f,
        });
      }

      if (visuals.mHasHitOnBoard && layerSprites.size() < MaxViewCount) {
        const uint32_t rad = std::clamp(minDim / 36u, 10u, 22u);
        const uint32_t diam = rad * 2u;
        const uint32_t rawX = dr.Left()
          + static_cast<uint32_t>(
            std::clamp(visuals.mHitLocalU, 0.0f, 1.0f) * dr.mSize.mWidth);
        const uint32_t rawY = dr.Top()
          + static_cast<uint32_t>(
            std::clamp(visuals.mHitLocalV, 0.0f, 1.0f) * dr.mSize.mHeight);
        const uint32_t rx = std::clamp(
          rawX > rad ? (rawX - rad) : dr.Left(),
          dr.Left(),
          dr.Right() > diam ? (dr.Right() - diam) : dr.Left());
        const uint32_t ry = std::clamp(
          rawY > rad ? (rawY - rad) : dr.Top(),
          dr.Top(),
          dr.Bottom() > diam ? (dr.Bottom() - diam) : dr.Top());
        layerSprites.push_back({
          .mSourceRect = gizmoReticleSrc,
          .mDestRect = {{rx, ry}, {diam, diam}},
          .mOpacity = 1.0f,
        });
      }
    }

    if (
      visuals.mHasRay && mBeamAtlasIndex
      && layerSprites.size() < MaxViewCount
      && mNextCompositionLayers.size() < mMaxLayerCount
      && addedXRLayers.size() < MaxViewCount) {
      const Vector3 beamVec = visuals.mRayEndWorld - visuals.mRayStartWorld;
      const float beamLen = beamVec.Length();
      if (beamLen > 0.03f) {
        const PixelRect beamDest = destSprites.GetRect(*mBeamAtlasIndex);
        layerSprites.push_back({
          .mSourceRect = gizmoBeamSrc,
          .mDestRect = beamDest,
          .mOpacity = 0.80f,
        });

        const Vector3 up = beamVec / beamLen;
        const Vector3 center =
          (visuals.mRayStartWorld + visuals.mRayEndWorld) * 0.5f;
        Vector3 toHmd = hmdPose.mPosition - center;
        if (toHmd.LengthSquared() < 1e-6f) {
          toHmd = Vector3::Backward;
        } else {
          toHmd.Normalize();
        }
        Vector3 right = up.Cross(toHmd);
        if (right.LengthSquared() < 1e-6f) {
          right = Vector3::UnitX;
        } else {
          right.Normalize();
        }
        Vector3 forward = right.Cross(up);
        forward.Normalize();

        Matrix rotMat(
          right.x, right.y, right.z, 0.0f,
          up.x, up.y, up.z, 0.0f,
          forward.x, forward.y, forward.z, 0.0f,
          0.0f, 0.0f, 0.0f, 1.0f);
        const Pose beamPose {
          .mPosition = center,
          .mOrientation = Quaternion::CreateFromRotationMatrix(rotMat),
        };
        addedXRLayers.push_back({
          .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
          .next = nullptr,
          .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
          .space = mLocalSpace,
          .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
          .subImage =
            XrSwapchainSubImage {
              .swapchain = mSwapchain,
              .imageRect = beamDest.StaticCast<int, XrRect2Di>(),
              .imageArrayIndex = 0,
            },
          .pose = this->GetXrPosef(beamPose),
          .size = {0.006f, beamLen},
        });
        mNextCompositionLayers.push_back(
          reinterpret_cast<XrCompositionLayerBaseHeader*>(
            &addedXRLayers.back()));
      }
    }
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
  nextFrameEndInfo.layers = mNextCompositionLayers.data();
  nextFrameEndInfo.layerCount =
    static_cast<uint32_t>(mNextCompositionLayers.size());

  XrResult nextResult {};
  {
    OPENKNEEBOARD_TraceLoggingScope("next_xrEndFrame");
    nextResult = mOpenXR->xrEndFrame(session, &nextFrameEndInfo);
  }
  if (!XR_SUCCEEDED(nextResult)) [[unlikely]] {
    const auto codeAsString = xrresult_to_string(nextResult);
    dprint.Warning("Next xrEndFrame failed: {}", codeAsString);
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

void OpenXRKneeboard::EnsureManipActionSetAttached() {
  if (
    !gManipActionSet || !mOpenXR->xrAttachSessionActionSets
    || mEndFrameCount < 10) {
    return;
  }
  {
    std::scoped_lock lock(gHandSpaceMutex);
    if (gAttachedSessions.contains(XrHandleKey(mSession))) {
      return;
    }
  }
  SuggestMissingManipBindings(mXRInstance);
  XrSessionActionSetsAttachInfo attachInfo {
    .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
    .next = nullptr,
    .countActionSets = 1,
    .actionSets = &gManipActionSet,
  };
  const auto res = mOpenXR->xrAttachSessionActionSets(mSession, &attachInfo);
  std::scoped_lock lock(gHandSpaceMutex);
  gAttachedSessions.insert(XrHandleKey(mSession));
  if (XR_SUCCEEDED(res)) {
    gAnySessionAttached = true;
    gManipAttachedSessions.insert(XrHandleKey(mSession));
    dprint("Attached OpenKneeboard standalone OpenXR controller ActionSet");
  } else {
    dprint.Warning(
      "Standalone xrAttachSessionActionSets returned {}",
      static_cast<int>(res));
  }
}

std::optional<OpenXRKneeboard::Pose> OpenXRKneeboard::LocateAppHandMountPose(
  const VRFollowMode mode,
  const XrTime displayTime) {
  if (mode != VRFollowMode::LeftHand && mode != VRFollowMode::RightHand) {
    return std::nullopt;
  }
  const size_t idx = (mode == VRFollowMode::LeftHand) ? 0 : 1;
  XrSpace space = mOwnGripSpaces[idx];
  if (!space) {
    std::scoped_lock lock(gHandSpaceMutex);
    if (SameXrHandle(gAppHandSpaces[idx].mSession, mSession)) {
      space = gAppHandSpaces[idx].mSpace;
    }
  }
  if (!space) {
    return std::nullopt;
  }
  XrSpaceLocation loc {.type = XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(
        mOpenXR->xrLocateSpace(space, mLocalSpace, displayTime, &loc))) {
    return std::nullopt;
  }
  constexpr auto kDesiredFlags =
    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
    | XR_SPACE_LOCATION_POSITION_VALID_BIT;
  if ((loc.locationFlags & kDesiredFlags) != kDesiredFlags) {
    return std::nullopt;
  }
  const auto& p = loc.pose.position;
  const auto& o = loc.pose.orientation;
  return Pose {
    .mPosition = {p.x, p.y, p.z},
    .mOrientation = {o.x, o.y, o.z, o.w},
  };
}

std::array<OpenXRKneeboard::ControllerInput, 2>
OpenXRKneeboard::BuildControllerInputs(
  const uint8_t preferredHand,
  const XrTime displayTime) {
  std::array<ControllerInput, 2> raw {};
  bool manipAttached = false;
  std::array<XrSpace, 2> appAimSpaces {};
  std::array<ControllerActionCache, 2> passiveStates {};
  {
    std::scoped_lock lock(gHandSpaceMutex);
    manipAttached = gManipAttachedSessions.contains(XrHandleKey(mSession));
    const auto now = std::chrono::steady_clock::now();
    for (size_t i = 0; i < 2; ++i) {
      if (SameXrHandle(gAppAimSpaces[i].mSession, mSession) && gAppAimSpaces[i].mSpace) {
        appAimSpaces[i] = gAppAimSpaces[i].mSpace;
      } else if (
        SameXrHandle(gAppHandSpaces[i].mSession, mSession) && gAppHandSpaces[i].mSpace) {
        appAimSpaces[i] = gAppHandSpaces[i].mSpace;
      }
      if ((now - gControllerStates[i].mLastActive) < std::chrono::milliseconds(500)) {
        passiveStates[i] = gControllerStates[i];
      }
    }
  }

  const std::array<XrPath, 2> handPaths {gLeftHandPath, gRightHandPath};
  constexpr auto kDesiredFlags =
    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
    | XR_SPACE_LOCATION_POSITION_VALID_BIT;

  for (size_t i = 0; i < 2; ++i) {
    auto& out = raw[i];
    out.mHandIndex = static_cast<uint8_t>(i);

    std::array<XrSpace, 3> candidates {
      mOwnAimSpaces[i],
      mOwnGripSpaces[i],
      appAimSpaces[i],
    };
    for (XrSpace sp : candidates) {
      if (!sp) {
        continue;
      }
      XrSpaceLocation loc {.type = XR_TYPE_SPACE_LOCATION};
      if (
        XR_SUCCEEDED(
          mOpenXR->xrLocateSpace(sp, mLocalSpace, displayTime, &loc))
        && ((loc.locationFlags & kDesiredFlags) == kDesiredFlags)) {
        const auto& p = loc.pose.position;
        const auto& o = loc.pose.orientation;
        out.mValid = true;
        out.mAimPose = {
          .mPosition = {p.x, p.y, p.z},
          .mOrientation = {o.x, o.y, o.z, o.w},
        };
        break;
      }
    }
    if (!out.mValid) {
      continue;
    }

    out.mGripHeld = passiveStates[i].mGripHeld;
    out.mTriggerHeld = passiveStates[i].mTriggerHeld;
    out.mThumbstickY = passiveStates[i].mThumbstickY;

    if (manipAttached && handPaths[i] != XR_NULL_PATH) {
      const auto readFloat = [&](XrAction act, float& val) {
        if (!act || !mOpenXR->xrGetActionStateFloat) {
          return;
        }
        XrActionStateGetInfo gi {
          .type = XR_TYPE_ACTION_STATE_GET_INFO,
          .next = nullptr,
          .action = act,
          .subactionPath = handPaths[i],
        };
        XrActionStateFloat st {.type = XR_TYPE_ACTION_STATE_FLOAT};
        if (
          XR_SUCCEEDED(mOpenXR->xrGetActionStateFloat(mSession, &gi, &st))
          && st.isActive) {
          val = st.currentState;
        }
      };
      const auto readBool = [&](XrAction act, bool& val) {
        if (!act || !mOpenXR->xrGetActionStateBoolean) {
          return;
        }
        XrActionStateGetInfo gi {
          .type = XR_TYPE_ACTION_STATE_GET_INFO,
          .next = nullptr,
          .action = act,
          .subactionPath = handPaths[i],
        };
        XrActionStateBoolean st {.type = XR_TYPE_ACTION_STATE_BOOLEAN};
        if (
          XR_SUCCEEDED(mOpenXR->xrGetActionStateBoolean(mSession, &gi, &st))
          && st.isActive && st.currentState == XR_TRUE) {
          val = true;
        }
      };
      const auto readVec2Y = [&](XrAction act, float& valY) {
        if (!act || !mOpenXR->xrGetActionStateVector2f) {
          return;
        }
        XrActionStateGetInfo gi {
          .type = XR_TYPE_ACTION_STATE_GET_INFO,
          .next = nullptr,
          .action = act,
          .subactionPath = handPaths[i],
        };
        XrActionStateVector2f st {.type = XR_TYPE_ACTION_STATE_VECTOR2F};
        if (
          XR_SUCCEEDED(mOpenXR->xrGetActionStateVector2f(mSession, &gi, &st))
          && st.isActive && std::abs(st.currentState.y) > std::abs(valY)) {
          valY = st.currentState.y;
        }
      };

      float gripVal = 0.0f;
      readFloat(gManipGripFloatAction, gripVal);
      if (gripVal > 0.55f) {
        out.mGripHeld = true;
      }
      readBool(gManipGripBoolAction, out.mGripHeld);

      float trigVal = 0.0f;
      readFloat(gManipTriggerFloatAction, trigVal);
      if (trigVal > 0.55f) {
        out.mTriggerHeld = true;
      }
      readBool(gManipSelectBoolAction, out.mTriggerHeld);

      readVec2Y(gManipStickVec2Action, out.mThumbstickY);
      readVec2Y(gManipTrackpadVec2Action, out.mThumbstickY);
    }
  }

  const size_t first = preferredHand % 2;
  return {raw[first], raw[1 - first]};
}

XrPosef OpenXRKneeboard::GetViewRelativeXrPosef(
  const VRHeadFollowSettings& follow) {
  const auto matrix =
    Matrix::CreateRotationX(DirectX::XMConvertToRadians(follow.mHeadPitchDeg))
    * Matrix::CreateRotationY(DirectX::XMConvertToRadians(follow.mHeadYawDeg))
    * Matrix::CreateTranslation({
      follow.mHeadOffsetX,
      follow.mHeadOffsetY,
      follow.mHeadOffsetZ,
    });
  const auto q = Quaternion::CreateFromRotationMatrix(matrix);
  const auto p = matrix.Translation();
  return {
    .orientation = {q.x, q.y, q.z, q.w},
    .position = {p.x, p.y, p.z},
  };
}

XrPosef OpenXRKneeboard::GetXrPosef(const Pose& pose) {
  const auto& p = pose.mPosition;
  const auto& o = pose.mOrientation;
  return {
    .orientation = {o.x, o.y, o.z, o.w},
    .position = {p.x, p.y, p.z},
  };
}
const SpriteAtlas& OpenXRKneeboard::GetSpriteAtlas(
  const SHM::Frame& frame) noexcept {
  const auto rawCacheKey =
    (static_cast<uint64_t>(frame.mConfig.mTextureSize.mWidth) << 32)
    | (static_cast<uint32_t>(frame.mConfig.mTextureSize.mHeight))
    ^ (static_cast<uint64_t>(frame.mLayers.size()) << 56);
  const auto upscale = this->IsUpscalingEnabled(frame.mConfig);
  const auto cacheKey = upscale ? rawCacheKey : ~rawCacheKey;
  if (mSpriteAtlasCacheKey == cacheKey) {
    return *mSpriteAtlas;
  }
  mSpriteAtlasCacheKey = cacheKey;
  inplace_vector<PixelSize, MaxViewCount> sizes;
  for (auto&& sprite : frame.mLayers) {
    sizes.push_back(
      upscale
        ? sprite.mVR.mLocationOnTexture.mSize.ScaledToFit(MaxViewRenderSize)
        : sprite.mVR.mLocationOnTexture.mSize);
  }
  if (sizes.size() < MaxViewCount) {
    mBeamAtlasIndex = static_cast<uint8_t>(sizes.size());
    sizes.push_back({32, 32});
  } else {
    mBeamAtlasIndex.reset();
  }
  mSpriteAtlas.emplace(sizes);
  return *mSpriteAtlas;
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
  dprint(
    "In xrCreateSession with OpenXR runtime: '{}' v{:#x}",
    gRuntime.mName,
    gRuntime.mVersion);

  const auto ret = Next()->xrCreateSession(instance, createInfo, session);
  if (XR_FAILED(ret)) {
    dprint.Warning("next xrCreateSession failed: {}", xrresult_to_string(ret));
    return ret;
  }
  dprint("Next xrCreateSession succeeded");

  if (gKneeboard) {
    dprint.Warning("Already have a kneeboard, refusing to initialize twice");
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

  dprint.Warning("Unsupported graphics API");
  for (auto next = reinterpret_cast<const XrBaseInStructure*>(createInfo); next;
       next = next->next) {
    switch (next->type) {
#define XR_TYPE_CASE(enum_name, value) \
  case enum_name: \
    dprint("xrCreateSession next chain: {}", #enum_name); \
    break;
      XR_LIST_ENUM_XrStructureType(XR_TYPE_CASE);
#undef XR_TYPE_CASE
      default:
        dprint(
          "xrCreateSession next chain: {}", std::to_underlying(next->type));
    }
  }

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

XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
  XrActionSet actionSet,
  const XrActionCreateInfo* createInfo,
  XrAction* action) {
  const auto ret = Next()->xrCreateAction(actionSet, createInfo, action);
  if (XR_SUCCEEDED(ret) && action && createInfo) {
    const std::string_view actionName {
      createInfo->actionName,
      strnlen_s(createInfo->actionName, XR_MAX_ACTION_NAME_SIZE)};
    bool hasLeft = false;
    bool hasRight = false;
    for (uint32_t i = 0; i < createInfo->countSubactionPaths; ++i) {
      if (createInfo->subactionPaths[i] == gLeftHandPath) {
        hasLeft = true;
      } else if (createInfo->subactionPaths[i] == gRightHandPath) {
        hasRight = true;
      }
    }
    if (!hasLeft && !hasRight) {
      if (ContainsCaseInsensitive(actionName, "left")) {
        hasLeft = true;
      } else if (ContainsCaseInsensitive(actionName, "right")) {
        hasRight = true;
      }
    }

    if (createInfo->actionType == XR_ACTION_TYPE_POSE_INPUT) {
      PoseActionInfo info {.mHasLeft = hasLeft, .mHasRight = hasRight};
      if (ContainsCaseInsensitive(actionName, "grip")) {
        info.mScore = 2;
      } else if (
        ContainsCaseInsensitive(actionName, "hand")
        || ContainsCaseInsensitive(actionName, "wrist")) {
        info.mScore = 1;
      }
      if (
        ContainsCaseInsensitive(actionName, "aim")
        || ContainsCaseInsensitive(actionName, "pointer")) {
        info.mAimScore = 3;
      } else {
        info.mAimScore = info.mScore;
      }
      if (info.mHasLeft || info.mHasRight) {
        std::scoped_lock lock(gHandSpaceMutex);
        gPoseActions[XrHandleKey(*action)] = info;
      }
    } else if (
      createInfo->actionType == XR_ACTION_TYPE_BOOLEAN_INPUT
      || createInfo->actionType == XR_ACTION_TYPE_FLOAT_INPUT) {
      ManipActionKind kind = ManipActionKind::None;
      if (
        ContainsCaseInsensitive(actionName, "grip")
        || ContainsCaseInsensitive(actionName, "squeeze")) {
        kind = ManipActionKind::Grip;
      } else if (
        ContainsCaseInsensitive(actionName, "trigger")
        || ContainsCaseInsensitive(actionName, "select")
        || ContainsCaseInsensitive(actionName, "click")) {
        kind = ManipActionKind::Trigger;
      }
      if (kind != ManipActionKind::None) {
        std::scoped_lock lock(gHandSpaceMutex);
        gManipActions[XrHandleKey(*action)] = {
          .mKind = kind,
          .mHasLeft = hasLeft,
          .mHasRight = hasRight,
        };
      }
    } else if (createInfo->actionType == XR_ACTION_TYPE_VECTOR2F_INPUT) {
      if (
        ContainsCaseInsensitive(actionName, "thumbstick")
        || ContainsCaseInsensitive(actionName, "joystick")
        || ContainsCaseInsensitive(actionName, "stick")
        || ContainsCaseInsensitive(actionName, "trackpad")) {
        std::scoped_lock lock(gHandSpaceMutex);
        gManipActions[XrHandleKey(*action)] = {
          .mKind = ManipActionKind::Thumbstick,
          .mHasLeft = hasLeft,
          .mHasRight = hasRight,
        };
      }
    }
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyAction(XrAction action) {
  {
    std::scoped_lock lock(gHandSpaceMutex);
    gPoseActions.erase(XrHandleKey(action));
    gManipActions.erase(XrHandleKey(action));
  }
  return Next()->xrDestroyAction(action);
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(
  XrSession session,
  const XrActionSpaceCreateInfo* createInfo,
  XrSpace* space) {
  const auto ret = Next()->xrCreateActionSpace(session, createInfo, space);
  if (XR_SUCCEEDED(ret) && space && createInfo) {
    std::scoped_lock lock(gHandSpaceMutex);
    int handIdx = -1;
    int score = 0;
    int aimScore = 0;
    if (const auto it = gPoseActions.find(XrHandleKey(createInfo->action));
        it != gPoseActions.end()) {
      score = it->second.mScore;
      aimScore = it->second.mAimScore;
      if (
        gLeftHandPath != XR_NULL_PATH
        && createInfo->subactionPath == gLeftHandPath) {
        handIdx = 0;
      } else if (
        gRightHandPath != XR_NULL_PATH
        && createInfo->subactionPath == gRightHandPath) {
        handIdx = 1;
      } else if (it->second.mHasLeft && !it->second.mHasRight) {
        handIdx = 0;
      } else if (it->second.mHasRight && !it->second.mHasLeft) {
        handIdx = 1;
      }
    }
    if (handIdx == 0 || handIdx == 1) {
      auto& slot = gAppHandSpaces[handIdx];
      if (!SameXrHandle(slot.mSession, session) || score >= slot.mScore) {
        slot = {
          .mSession = session,
          .mSpace = *space,
          .mScore = score,
        };
      }
      auto& aimSlot = gAppAimSpaces[handIdx];
      if (!SameXrHandle(aimSlot.mSession, session) || aimScore >= aimSlot.mScore) {
        aimSlot = {
          .mSession = session,
          .mSpace = *space,
          .mScore = aimScore,
        };
      }
    }
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(XrSpace space) {
  {
    std::scoped_lock lock(gHandSpaceMutex);
    for (auto& slot : gAppHandSpaces) {
      if (SameXrHandle(slot.mSpace, space)) {
        slot = {};
      }
    }
    for (auto& slot : gAppAimSpaces) {
      if (SameXrHandle(slot.mSpace, space)) {
        slot = {};
      }
    }
  }
  return Next()->xrDestroySpace(space);
}

XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(
  XrInstance instance,
  const XrInteractionProfileSuggestedBinding* suggestedBindings) {
  if (!suggestedBindings) {
    return Next()->xrSuggestInteractionProfileBindings(
      instance, suggestedBindings);
  }
  if (gManipActionSet) {
    {
      std::scoped_lock lock(gHandSpaceMutex);
      gGameSuggestedProfiles.insert(suggestedBindings->interactionProfile);
    }
    const auto extra = GetManipBindingsForProfilePath(
      instance, suggestedBindings->interactionProfile);
    if (!extra.empty()) {
      std::vector<XrActionSuggestedBinding> merged;
      merged.reserve(suggestedBindings->countSuggestedBindings + extra.size());
      for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
        merged.push_back(suggestedBindings->suggestedBindings[i]);
      }
      merged.insert(merged.end(), extra.begin(), extra.end());
      auto modified = *suggestedBindings;
      modified.countSuggestedBindings = static_cast<uint32_t>(merged.size());
      modified.suggestedBindings = merged.data();
      const auto res =
        Next()->xrSuggestInteractionProfileBindings(instance, &modified);
      if (XR_SUCCEEDED(res)) {
        return res;
      }
      dprint.Warning(
        "Merged xrSuggestInteractionProfileBindings failed ({}); retrying "
        "application bindings alone",
        static_cast<int>(res));
    }
  }
  return Next()->xrSuggestInteractionProfileBindings(
    instance, suggestedBindings);
}

XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(
  XrSession session,
  const XrSessionActionSetsAttachInfo* attachInfo) {
  if (!attachInfo) {
    return Next()->xrAttachSessionActionSets(session, attachInfo);
  }
  SuggestMissingManipBindings(gXRInstance);
  if (gManipActionSet) {
    std::vector<XrActionSet> sets;
    sets.reserve(attachInfo->countActionSets + 1);
    bool hasOurs = false;
    for (uint32_t i = 0; i < attachInfo->countActionSets; ++i) {
      sets.push_back(attachInfo->actionSets[i]);
      if (SameXrHandle(attachInfo->actionSets[i], gManipActionSet)) {
        hasOurs = true;
      }
    }
    if (!hasOurs) {
      sets.push_back(gManipActionSet);
    }
    auto modified = *attachInfo;
    modified.countActionSets = static_cast<uint32_t>(sets.size());
    modified.actionSets = sets.data();
    const auto res = Next()->xrAttachSessionActionSets(session, &modified);
    if (XR_SUCCEEDED(res)) {
      std::scoped_lock lock(gHandSpaceMutex);
      gAnySessionAttached = true;
      gAttachedSessions.insert(XrHandleKey(session));
      gManipAttachedSessions.insert(XrHandleKey(session));
      return res;
    }
    dprint.Warning(
      "Merged xrAttachSessionActionSets failed ({}); retrying app sets",
      static_cast<int>(res));
  }
  const auto res = Next()->xrAttachSessionActionSets(session, attachInfo);
  if (XR_SUCCEEDED(res)) {
    std::scoped_lock lock(gHandSpaceMutex);
    gAnySessionAttached = true;
    gAttachedSessions.insert(XrHandleKey(session));
  }
  return res;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(
  XrSession session,
  const XrActionsSyncInfo* syncInfo) {
  if (!syncInfo) {
    return Next()->xrSyncActions(session, syncInfo);
  }
  bool attachManip = false;
  {
    std::scoped_lock lock(gHandSpaceMutex);
    attachManip = gManipAttachedSessions.contains(XrHandleKey(session));
  }
  if (attachManip && gManipActionSet) {
    std::vector<XrActiveActionSet> activeSets;
    activeSets.reserve(syncInfo->countActiveActionSets + 1);
    bool hasOurs = false;
    for (uint32_t i = 0; i < syncInfo->countActiveActionSets; ++i) {
      activeSets.push_back(syncInfo->activeActionSets[i]);
      if (SameXrHandle(syncInfo->activeActionSets[i].actionSet, gManipActionSet)) {
        hasOurs = true;
      }
    }
    if (!hasOurs) {
      activeSets.push_back({gManipActionSet, XR_NULL_PATH});
    }
    auto modified = *syncInfo;
    modified.countActiveActionSets = static_cast<uint32_t>(activeSets.size());
    modified.activeActionSets = activeSets.data();
    const auto res = Next()->xrSyncActions(session, &modified);
    if (XR_SUCCEEDED(res)) {
      std::scoped_lock lock(gHandSpaceMutex);
      gSyncedManipThisFrame[XrHandleKey(session)] = true;
    }
    return res;
  }
  return Next()->xrSyncActions(session, syncInfo);
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession session) {
  delete gKneeboard;
  gKneeboard = nullptr;
  {
    std::scoped_lock lock(gHandSpaceMutex);
    gAttachedSessions.erase(XrHandleKey(session));
    gManipAttachedSessions.erase(XrHandleKey(session));
    gSyncedManipThisFrame.erase(XrHandleKey(session));
    for (auto& slot : gAppHandSpaces) {
      if (SameXrHandle(slot.mSession, session)) {
        slot = {};
      }
    }
    for (auto& slot : gAppAimSpaces) {
      if (SameXrHandle(slot.mSession, session)) {
        slot = {};
      }
    }
  }
  return Next()->xrDestroySession(session);
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(XrInstance instance) {
  delete gKneeboard;
  gKneeboard = nullptr;
  {
    std::scoped_lock lock(gHandSpaceMutex);
    DestroyManipActions();
    gPoseActions.clear();
    gManipActions.clear();
    gAppHandSpaces = {};
    gAppAimSpaces = {};
    gControllerStates = {};
  }
  gXRInstance = XR_NULL_HANDLE;
  gLeftHandPath = XR_NULL_PATH;
  gRightHandPath = XR_NULL_PATH;
  return Next()->xrDestroyInstance(instance);
}

static int ResolveActionHandIndex(
  const XrActionStateGetInfo* getInfo,
  const ManipActionInfo& info) {
  if (gLeftHandPath != XR_NULL_PATH && getInfo->subactionPath == gLeftHandPath) {
    return 0;
  }
  if (
    gRightHandPath != XR_NULL_PATH
    && getInfo->subactionPath == gRightHandPath) {
    return 1;
  }
  if (info.mHasLeft && !info.mHasRight) {
    return 0;
  }
  if (info.mHasRight && !info.mHasLeft) {
    return 1;
  }
  return 1;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(
  XrSession session,
  const XrActionStateGetInfo* getInfo,
  XrActionStateBoolean* state) {
  const auto ret = Next()->xrGetActionStateBoolean(session, getInfo, state);
  if (XR_SUCCEEDED(ret) && getInfo && state) {
    std::scoped_lock lock(gHandSpaceMutex);
    if (const auto it = gManipActions.find(XrHandleKey(getInfo->action));
        it != gManipActions.end()) {
      const int hand = ResolveActionHandIndex(getInfo, it->second);
      auto& st = gControllerStates[hand];
      const bool pressed = state->isActive && (state->currentState == XR_TRUE);
      if (state->isActive) {
        st.mLastActive = std::chrono::steady_clock::now();
      }
      if (it->second.mKind == ManipActionKind::Grip) {
        st.mGripHeld = pressed;
      } else if (it->second.mKind == ManipActionKind::Trigger) {
        st.mTriggerHeld = pressed;
      }
    }
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(
  XrSession session,
  const XrActionStateGetInfo* getInfo,
  XrActionStateFloat* state) {
  const auto ret = Next()->xrGetActionStateFloat(session, getInfo, state);
  if (XR_SUCCEEDED(ret) && getInfo && state) {
    std::scoped_lock lock(gHandSpaceMutex);
    if (const auto it = gManipActions.find(XrHandleKey(getInfo->action));
        it != gManipActions.end()) {
      const int hand = ResolveActionHandIndex(getInfo, it->second);
      auto& st = gControllerStates[hand];
      const bool pressed = state->isActive && (state->currentState > 0.60f);
      if (state->isActive) {
        st.mLastActive = std::chrono::steady_clock::now();
      }
      if (it->second.mKind == ManipActionKind::Grip) {
        st.mGripHeld = pressed;
      } else if (it->second.mKind == ManipActionKind::Trigger) {
        st.mTriggerHeld = pressed;
      }
    }
  }
  return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(
  XrSession session,
  const XrActionStateGetInfo* getInfo,
  XrActionStateVector2f* state) {
  const auto ret = Next()->xrGetActionStateVector2f(session, getInfo, state);
  if (XR_SUCCEEDED(ret) && getInfo && state && state->isActive) {
    std::scoped_lock lock(gHandSpaceMutex);
    if (const auto it = gManipActions.find(XrHandleKey(getInfo->action));
        it != gManipActions.end()) {
      if (it->second.mKind == ManipActionKind::Thumbstick) {
        const int hand = ResolveActionHandIndex(getInfo, it->second);
        auto& st = gControllerStates[hand];
        st.mLastActive = std::chrono::steady_clock::now();
        st.mThumbstickY = state->currentState.y;
      }
    }
  }
  return ret;
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
  constexpr std::string_view kHandTrackingDataSourceExt {
    "XR_EXT_hand_tracking_data_source"};
  bool hasHandTrackingExt = false;
  bool hasDataSourceExt = false;
  std::vector<const char*> requestedExts;
  requestedExts.reserve(info->enabledExtensionCount + 2);
  for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
    requestedExts.push_back(info->enabledExtensionNames[i]);
    const std::string_view extName {info->enabledExtensionNames[i]};
    if (extName == XR_EXT_HAND_TRACKING_EXTENSION_NAME) {
      hasHandTrackingExt = true;
    } else if (extName == kHandTrackingDataSourceExt) {
      hasDataSourceExt = true;
    }
  }

  XrResult nextResult = XR_ERROR_EXTENSION_NOT_PRESENT;
  gHaveXR_EXT_hand_tracking_data_source = hasDataSourceExt;

  if (!hasHandTrackingExt || !hasDataSourceExt) {
    auto extsWithDataSource = requestedExts;
    if (!hasHandTrackingExt) {
      extsWithDataSource.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
    }
    if (!hasDataSourceExt) {
      extsWithDataSource.push_back(kHandTrackingDataSourceExt.data());
    }
    XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
    nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
    auto modifiedInfo = *info;
    modifiedInfo.enabledExtensionCount =
      static_cast<uint32_t>(extsWithDataSource.size());
    modifiedInfo.enabledExtensionNames = extsWithDataSource.data();
    nextResult = layerInfo->nextInfo->nextCreateApiLayerInstance(
      &modifiedInfo, &nextLayerInfo, instance);
    if (XR_SUCCEEDED(nextResult)) {
      gHaveXR_EXT_hand_tracking_data_source = true;
    }
  }

  if (nextResult == XR_ERROR_EXTENSION_NOT_PRESENT && !hasHandTrackingExt) {
    auto extsHandOnly = requestedExts;
    extsHandOnly.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
    XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
    nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
    auto modifiedInfo = *info;
    modifiedInfo.enabledExtensionCount =
      static_cast<uint32_t>(extsHandOnly.size());
    modifiedInfo.enabledExtensionNames = extsHandOnly.data();
    nextResult = layerInfo->nextInfo->nextCreateApiLayerInstance(
      &modifiedInfo, &nextLayerInfo, instance);
    if (XR_SUCCEEDED(nextResult)) {
      gHaveXR_EXT_hand_tracking_data_source = hasDataSourceExt;
    }
  }

  if (nextResult == XR_ERROR_EXTENSION_NOT_PRESENT || (hasHandTrackingExt && hasDataSourceExt)) {
    XrApiLayerCreateInfo nextLayerInfo = *layerInfo;
    nextLayerInfo.nextInfo = layerInfo->nextInfo->next;
    nextResult = layerInfo->nextInfo->nextCreateApiLayerInstance(
      info, &nextLayerInfo, instance);
    if (XR_SUCCEEDED(nextResult)) {
      gHaveXR_EXT_hand_tracking_data_source = hasDataSourceExt;
    }
  }
  if (XR_FAILED(nextResult)) {
    dprint.Warning(
      "Next xrCreateApiLayerInstance failed: {}",
      xrresult_to_string(nextResult));
    return nextResult;
  }

  gHaveXR_KHR_vulkan_enable2 = false;

  for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
    const std::string_view extensionName {info->enabledExtensionNames[i]};
    dprint("Application enabled extension: {}", extensionName);
    if (extensionName == XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME) {
      dprint.Warning(
        "Application enabled XR_KHR_COMPOSITION_LAYER_DEPTH; runtime bugs are "
        "likely");
      continue;
    }
    if (extensionName == XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) {
      gHaveXR_KHR_vulkan_enable2 = true;
      continue;
    }
  }

  {
    std::scoped_lock lock(gHandSpaceMutex);
    DestroyManipActions();
    gPoseActions.clear();
    gManipActions.clear();
    gAppHandSpaces = {};
    gAppAimSpaces = {};
    gControllerStates = {};
  }
  Next() = std::make_shared<OpenXRNext>(
    *instance, layerInfo->nextInfo->nextGetInstanceProcAddr);
  gXRInstance = *instance;
  gLeftHandPath = XR_NULL_PATH;
  gRightHandPath = XR_NULL_PATH;
  if (Next()->xrStringToPath) {
    Next()->xrStringToPath(*instance, "/user/hand/left", &gLeftHandPath);
    Next()->xrStringToPath(*instance, "/user/hand/right", &gRightHandPath);
  }
  InitManipActions(*instance);

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
