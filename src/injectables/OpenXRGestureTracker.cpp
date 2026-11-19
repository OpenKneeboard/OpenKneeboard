// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRGestureTracker.hpp"
#include "OpenXRNext.hpp"

#include <OpenKneeboard/dprint.hpp>

#include <cmath>
#include <cstdint>

#ifndef XR_EXT_hand_tracking_data_source
#define XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME \
  "XR_EXT_hand_tracking_data_source"

enum XrHandTrackingDataSourceEXT : int32_t {
  XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT = 1,
  XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT = 2,
};

struct XrHandTrackingDataSourceInfoEXT {
  XrStructureType type;
  const void* next;
  uint32_t requestedDataSourceCount;
  const XrHandTrackingDataSourceEXT* requestedDataSources;
};

struct XrSystemHandTrackingDataSourcePropertiesEXT {
  XrStructureType type;
  void* next;
  XrBool32 supportsDataSource;
};

constexpr auto XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT =
  static_cast<XrStructureType>(1000428000);
constexpr auto XR_TYPE_SYSTEM_HAND_TRACKING_DATA_SOURCE_PROPERTIES_EXT =
  static_cast<XrStructureType>(1000428002);
#endif

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

static Vector3 ToVec3(const XrVector3f& v) { return {v.x, v.y, v.z}; }
static Quaternion ToQuat(const XrQuaternionf& q) {
  return {q.x, q.y, q.z, q.w};
}

OpenXRGestureTracker::OpenXRGestureTracker(
  XrInstance instance,
  XrSystemId systemId,
  XrSession session,
  const std::shared_ptr<OpenXRNext>& next,
  const bool dataSourceExtensionEnabled)
  : mOpenXR(next) {
  XrSystemHandTrackingDataSourcePropertiesEXT dataSourceProps {
    .type = XR_TYPE_SYSTEM_HAND_TRACKING_DATA_SOURCE_PROPERTIES_EXT,
    .next = nullptr,
    .supportsDataSource = XR_FALSE,
  };
  XrSystemHandTrackingPropertiesEXT handProps {
    .type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT,
    .next = dataSourceExtensionEnabled ? &dataSourceProps : nullptr,
  };
  XrSystemProperties sysProps {
    .type = XR_TYPE_SYSTEM_PROPERTIES,
    .next = &handProps,
  };
  if (
    XR_FAILED(next->xrGetSystemProperties(instance, systemId, &sysProps))
    || !handProps.supportsHandTracking) {
    dprint.Warning(
      "XR_EXT_hand_tracking unavailable on this OpenXR system (supports={}). "
      "If using Virtual Desktop/VDXR, enable 'Forward tracking data to PC' "
      "or use controller poses.",
      static_cast<bool>(handProps.supportsHandTracking));
    return;
  }

  next->xrGetInstanceProcAddr(
    instance,
    "xrCreateHandTrackerEXT",
    reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateHandTrackerEXT));
  next->xrGetInstanceProcAddr(
    instance,
    "xrDestroyHandTrackerEXT",
    reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyHandTrackerEXT));
  next->xrGetInstanceProcAddr(
    instance,
    "xrLocateHandJointsEXT",
    reinterpret_cast<PFN_xrVoidFunction*>(&xrLocateHandJointsEXT));

  if (!xrCreateHandTrackerEXT || !xrLocateHandJointsEXT) {
    dprint.Warning("OpenXR runtime did not expose xrLocateHandJointsEXT");
    return;
  }

  constexpr std::array<XrHandTrackingDataSourceEXT, 2> kDataSources {
    XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT,
    XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT,
  };
  const XrHandTrackingDataSourceInfoEXT dataSourceInfo {
    .type = XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT,
    .next = nullptr,
    .requestedDataSourceCount = static_cast<uint32_t>(kDataSources.size()),
    .requestedDataSources = kDataSources.data(),
  };
  const bool useDataSource =
    dataSourceExtensionEnabled && dataSourceProps.supportsDataSource;

  bool anyCreated = false;
  for (size_t i = 0; i < 2; ++i) {
    XrHandTrackerCreateInfoEXT createInfo {
      .type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
      .next = useDataSource ? &dataSourceInfo : nullptr,
      .hand = (i == 0) ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT,
      .handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
    };
    XrResult createResult = xrCreateHandTrackerEXT(
      session, &createInfo, &mHands[i].mTracker);
    if (XR_FAILED(createResult) && useDataSource) {
      createInfo.next = nullptr;
      createResult = xrCreateHandTrackerEXT(
        session, &createInfo, &mHands[i].mTracker);
    }
    if (XR_SUCCEEDED(createResult)) {
      anyCreated = true;
    } else {
      mHands[i].mTracker = XR_NULL_HANDLE;
    }
  }
  mHandTrackingSupported = anyCreated;
  if (anyCreated) {
    dprint(
      "Initialized OpenXR XR_EXT_hand_tracking tracker (data_source={})",
      useDataSource);
  }
}

OpenXRGestureTracker::~OpenXRGestureTracker() {
  if (!xrDestroyHandTrackerEXT) {
    return;
  }
  for (auto& h : mHands) {
    if (h.mTracker) {
      xrDestroyHandTrackerEXT(h.mTracker);
    }
  }
}

void OpenXRGestureTracker::DispatchAction(UserAction action) {
  APIEvent {APIEvent::EVT_REMOTE_USER_ACTION, to_string(action)}.Send();
}

void OpenXRGestureTracker::LocateHands(
  XrSpace locateSpace,
  XrTime displayTime) {
  if (!mHandTrackingSupported || !xrLocateHandJointsEXT) {
    return;
  }
  for (auto& hand : mHands) {
    hand.mLocated = false;
    hand.mWristPose.reset();
    if (!hand.mTracker) {
      continue;
    }
    XrHandJointLocationsEXT locations {
      .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
      .jointCount = XR_HAND_JOINT_COUNT_EXT,
      .jointLocations = hand.mJoints.data(),
    };
    XrHandJointsLocateInfoEXT locateInfo {
      .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
      .baseSpace = locateSpace,
      .time = displayTime,
    };
    if (
      XR_FAILED(xrLocateHandJointsEXT(hand.mTracker, &locateInfo, &locations))
      || !locations.isActive) {
      continue;
    }
    hand.mLocated = true;
    const auto& wrist = hand.mJoints[XR_HAND_JOINT_WRIST_EXT];
    const auto& palm = hand.mJoints[XR_HAND_JOINT_PALM_EXT];
    constexpr auto kValidPoseBits =
      XR_SPACE_LOCATION_POSITION_VALID_BIT
      | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((wrist.locationFlags & kValidPoseBits) == kValidPoseBits) {
      hand.mWristPose = VRKneeboard::Pose {
        .mPosition = ToVec3(wrist.pose.position),
        .mOrientation = ToQuat(wrist.pose.orientation),
      };
    } else if ((palm.locationFlags & kValidPoseBits) == kValidPoseBits) {
      hand.mWristPose = VRKneeboard::Pose {
        .mPosition = ToVec3(palm.pose.position),
        .mOrientation = ToQuat(palm.pose.orientation),
      };
    }
    if (hand.mWristPose && !mLoggedFirstActiveHand) {
      mLoggedFirstActiveHand = true;
      dprint("Acquired active OpenXR hand pose for wrist pinning");
    }
  }
}

std::optional<VRKneeboard::ControllerInput> OpenXRGestureTracker::GetHandRayInput(
  const uint8_t preferredHand) const noexcept {
  if (!mHandTrackingSupported) {
    return std::nullopt;
  }
  const std::array<size_t, 2> order {
    static_cast<size_t>(preferredHand % 2),
    static_cast<size_t>(1 - (preferredHand % 2)),
  };
  constexpr auto kPosBit = XR_SPACE_LOCATION_POSITION_VALID_BIT;
  for (const size_t i : order) {
    const auto& hand = mHands[i];
    if (!hand.mLocated) {
      continue;
    }
    const auto& prox = hand.mJoints[XR_HAND_JOINT_INDEX_PROXIMAL_EXT];
    const auto& tip = hand.mJoints[XR_HAND_JOINT_INDEX_TIP_EXT];
    if (!(prox.locationFlags & kPosBit) || !(tip.locationFlags & kPosBit)) {
      continue;
    }
    const Vector3 p0 = ToVec3(prox.pose.position);
    const Vector3 p1 = ToVec3(tip.pose.position);
    Vector3 dir = p1 - p0;
    if (dir.LengthSquared() < 1e-6f) {
      continue;
    }
    dir.Normalize();
    const Vector3 upVec =
      (std::abs(dir.y) > 0.96f) ? Vector3::UnitZ : Vector3::Up;
    // Construct orientation whose Vector3::Forward (-Z) equals dir
    const Quaternion aimRot = Quaternion::CreateFromRotationMatrix(
      Matrix::CreateWorld(Vector3::Zero, dir, upVec));
    bool pinching = hand.mIsPinching;
    const auto& thumbTip = hand.mJoints[XR_HAND_JOINT_THUMB_TIP_EXT];
    if ((thumbTip.locationFlags & kPosBit) != 0) {
      const float pinchDist = Vector3::Distance(ToVec3(thumbTip.pose.position), p1);
      pinching = pinchDist <= 0.028f;
    }
    return VRKneeboard::ControllerInput {
      .mValid = true,
      .mHandIndex = static_cast<uint8_t>(i),
      .mAimPose = {.mPosition = p1, .mOrientation = aimRot},
      .mGripHeld = false,
      .mTriggerHeld = pinching,
      .mThumbstickY = 0.0f,
    };
  }
  return std::nullopt;
}

std::optional<VRKneeboard::Pose> OpenXRGestureTracker::GetHandMountPose(
  const VRFollowMode mode) const noexcept {
  if (!mHandTrackingSupported) {
    return std::nullopt;
  }
  if (mode == VRFollowMode::LeftHand) {
    return mHands[0].mWristPose;
  }
  if (mode == VRFollowMode::RightHand) {
    return mHands[1].mWristPose;
  }
  return std::nullopt;
}

void OpenXRGestureTracker::PollLocatedGestures(
  const VRKneeboard::Pose& hmdPose,
  const std::optional<VRKneeboard::Layer>& /*activeLayer*/,
  const VRGestureSettings& settings) {
  if (!mHandTrackingSupported || !settings.mEnabled) {
    return;
  }
  for (auto& hand : mHands) {
    if (hand.mLocated) {
      this->ProcessHand(hand, hmdPose, settings);
    }
  }
}

void OpenXRGestureTracker::ProcessHand(
  HandState& hand,
  const VRKneeboard::Pose& hmdPose,
  const VRGestureSettings& settings) {
  const auto& palm = hand.mJoints[XR_HAND_JOINT_PALM_EXT];
  const auto& thumbTip = hand.mJoints[XR_HAND_JOINT_THUMB_TIP_EXT];
  const auto& indexTip = hand.mJoints[XR_HAND_JOINT_INDEX_TIP_EXT];
  if (!(palm.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
    return;
  }

  const auto palmPos = ToVec3(palm.pose.position);
  Quaternion invHead;
  hmdPose.mOrientation.Conjugate(invHead);
  const Vector3 localPos = Vector3::Transform(
    palmPos - hmdPose.mPosition, invHead);

  const auto now = std::chrono::steady_clock::now();
  const auto cooldown = std::chrono::milliseconds(settings.mCooldownMs);
  const bool inCooldown = (now - hand.mLastActionTime) < cooldown;

  const float pinchDist = Vector3::Distance(
    ToVec3(thumbTip.pose.position), ToVec3(indexTip.pose.position));
  const bool isPinching = pinchDist <= settings.mPinchThresholdM;

  if (isPinching && !hand.mIsPinching) {
    hand.mPinchStart = now;
    hand.mPinchStartLocalPos = localPos;
    hand.mPinchMoved = false;
  } else if (isPinching && settings.mAllowPinchReposition) {
    if (Vector3::Distance(localPos, hand.mPinchStartLocalPos) > 0.025f) {
      hand.mPinchMoved = true;
    }
  } else if (!isPinching && hand.mIsPinching && !hand.mPinchMoved && !inCooldown) {
    if ((now - hand.mPinchStart) < std::chrono::milliseconds(380)) {
      this->DispatchAction(UserAction::TOGGLE_FORCE_ZOOM);
      hand.mLastActionTime = now;
    }
  }
  hand.mIsPinching = isPinching;

  const auto palmNormal = Vector3::Transform(
    Vector3::Down, ToQuat(palm.pose.orientation));
  auto toHmd = hmdPose.mPosition - palmPos;
  if (toHmd.LengthSquared() > 1e-6f) {
    toHmd.Normalize();
  }
  if (!isPinching && palmNormal.Dot(toHmd) > 0.78f) {
    if (!hand.mPalmHoldStart) {
      hand.mPalmHoldStart = now;
    } else if (
      (now - *hand.mPalmHoldStart) >= std::chrono::milliseconds(550)
      && !inCooldown) {
      this->DispatchAction(UserAction::TOGGLE_VISIBILITY);
      hand.mLastActionTime = now;
      hand.mPalmHoldStart.reset();
    }
  } else {
    hand.mPalmHoldStart.reset();
  }

  hand.mTrail.push_back({localPos, now});
  while (
    !hand.mTrail.empty()
    && (now - hand.mTrail.front().mTime) > std::chrono::milliseconds(480)) {
    hand.mTrail.pop_front();
  }

  if (!isPinching && !inCooldown && hand.mTrail.size() >= 3) {
    const auto delta = localPos - hand.mTrail.front().mLocalPos;
    if (
      std::abs(delta.x) >= settings.mSwipeMinDistanceM
      && std::abs(delta.x) > std::abs(delta.y) * 1.35f) {
      this->DispatchAction(
        delta.x < 0 ? UserAction::NEXT_PAGE : UserAction::PREVIOUS_PAGE);
      hand.mLastActionTime = now;
      hand.mTrail.clear();
    } else if (
      std::abs(delta.y) >= settings.mSwipeMinDistanceM
      && std::abs(delta.y) > std::abs(delta.x) * 1.35f) {
      this->DispatchAction(
        delta.y > 0 ? UserAction::NEXT_TAB : UserAction::PREVIOUS_TAB);
      hand.mLastActionTime = now;
      hand.mTrail.clear();
    }
  }
}

}// namespace OpenKneeboard
