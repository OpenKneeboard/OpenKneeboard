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

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

static Vector3 ToVec3(const XrVector3f& v) { return {v.x, v.y, v.z}; }
static Quaternion ToQuat(const XrQuaternionf& q) { return {q.x, q.y, q.z, q.w}; }

OpenXRGestureTracker::OpenXRGestureTracker(
  XrInstance instance,
  XrSystemId systemId,
  XrSession session,
  const std::shared_ptr<OpenXRNext>& next)
  : mOpenXR(next) {
  XrSystemHandTrackingPropertiesEXT handProps {
    .type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT,
  };
  XrSystemProperties sysProps {
    .type = XR_TYPE_SYSTEM_PROPERTIES,
    .next = &handProps,
  };
  if (XR_FAILED(next->xrGetSystemProperties(instance, systemId, &sysProps))
      || !handProps.supportsHandTracking) {
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
    return;
  }

  bool anyCreated = false;
  for (size_t i = 0; i < 2; ++i) {
    XrHandTrackerCreateInfoEXT createInfo {
      .type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
      .hand = (i == 0) ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT,
      .handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
    };
    if (XR_SUCCEEDED(xrCreateHandTrackerEXT(session, &createInfo, &mHands[i].mTracker))) {
      anyCreated = true;
    } else {
      mHands[i].mTracker = XR_NULL_HANDLE;
    }
  }
  mHandTrackingSupported = anyCreated;
  if (anyCreated) {
    dprint("Initialized OpenXR XR_EXT_hand_tracking gesture tracker");
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

void OpenXRGestureTracker::PollAndDispatch(
  XrSpace locateSpace,
  XrTime displayTime,
  const VRKneeboard::Pose& hmdPose,
  const std::optional<VRKneeboard::Layer>& activeLayer,
  const VRGestureSettings& settings) {
  if (!mHandTrackingSupported || !settings.mEnabled) {
    return;
  }
  for (auto& hand : mHands) {
    if (hand.mTracker) {
      this->ProcessHand(hand, locateSpace, displayTime, hmdPose, activeLayer, settings);
    }
  }
}

void OpenXRGestureTracker::ProcessHand(
  HandState& hand,
  XrSpace locateSpace,
  XrTime displayTime,
  const VRKneeboard::Pose& hmdPose,
  const std::optional<VRKneeboard::Layer>&,
  const VRGestureSettings& settings) {
  std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> joints {};
  XrHandJointLocationsEXT locations {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = XR_HAND_JOINT_COUNT_EXT,
    .jointLocations = joints.data(),
  };
  XrHandJointsLocateInfoEXT locateInfo {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = locateSpace,
    .time = displayTime,
  };
  if (XR_FAILED(xrLocateHandJointsEXT(hand.mTracker, &locateInfo, &locations))
      || !locations.isActive) {
    return;
  }

  const auto& palm = joints[XR_HAND_JOINT_PALM_EXT];
  const auto& thumbTip = joints[XR_HAND_JOINT_THUMB_TIP_EXT];
  const auto& indexTip = joints[XR_HAND_JOINT_INDEX_TIP_EXT];
  if (!(palm.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  const auto cooldown = std::chrono::milliseconds(settings.mCooldownMs);
  const bool inCooldown = (now - hand.mLastActionTime) < cooldown;

  const float pinchDist = Vector3::Distance(
    ToVec3(thumbTip.pose.position), ToVec3(indexTip.pose.position));
  const bool isPinching = pinchDist <= settings.mPinchThresholdM;

  if (!isPinching && hand.mIsPinching && !hand.mPinchMoved && !inCooldown) {
    if ((now - hand.mPinchStart) < std::chrono::milliseconds(380)) {
      this->DispatchAction(UserAction::TOGGLE_FORCE_ZOOM);
      hand.mLastActionTime = now;
    }
  }
  if (isPinching && !hand.mIsPinching) {
    hand.mPinchStart = now;
    hand.mPinchMoved = false;
  }
  hand.mIsPinching = isPinching;

  const auto palmPos = ToVec3(palm.pose.position);
  const auto palmNormal = Vector3::Transform(
    Vector3::Down, ToQuat(palm.pose.orientation));
  auto toHmd = hmdPose.mPosition - palmPos;
  if (toHmd.LengthSquared() > 1e-6f) {
    toHmd.Normalize();
  }
  if (!isPinching && palmNormal.Dot(toHmd) > 0.78f) {
    if (!hand.mPalmHoldStart) {
      hand.mPalmHoldStart = now;
    } else if ((now - *hand.mPalmHoldStart) >= std::chrono::milliseconds(550) && !inCooldown) {
      this->DispatchAction(UserAction::TOGGLE_VISIBILITY);
      hand.mLastActionTime = now;
      hand.mPalmHoldStart.reset();
    }
  } else {
    hand.mPalmHoldStart.reset();
  }

  Quaternion invHead;
  hmdPose.mOrientation.Conjugate(invHead);
  const Vector3 localPos = Vector3::Transform(palmPos - hmdPose.mPosition, invHead);
  hand.mTrail.push_back({localPos, now});
  while (!hand.mTrail.empty()
         && (now - hand.mTrail.front().mTime) > std::chrono::milliseconds(480)) {
    hand.mTrail.pop_front();
  }

  if (!isPinching && !inCooldown && hand.mTrail.size() >= 3) {
    const auto delta = localPos - hand.mTrail.front().mLocalPos;
    if (std::abs(delta.x) >= settings.mSwipeMinDistanceM
        && std::abs(delta.x) > std::abs(delta.y) * 1.35f) {
      this->DispatchAction(
        delta.x < 0 ? UserAction::NEXT_PAGE : UserAction::PREVIOUS_PAGE);
      hand.mLastActionTime = now;
      hand.mTrail.clear();
    } else if (std::abs(delta.y) >= settings.mSwipeMinDistanceM
               && std::abs(delta.y) > std::abs(delta.x) * 1.35f) {
      this->DispatchAction(
        delta.y > 0 ? UserAction::NEXT_TAB : UserAction::PREVIOUS_TAB);
      hand.mLastActionTime = now;
      hand.mTrail.clear();
    }
  }
}

}// namespace OpenKneeboard
