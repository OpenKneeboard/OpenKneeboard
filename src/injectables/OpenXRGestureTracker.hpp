// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#pragma once

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/UserAction.hpp>
#include <OpenKneeboard/VRKneeboard.hpp>
#include <OpenKneeboard/VRSettings.hpp>

#include <shims/openxr/openxr.h>

#include <array>
#include <chrono>
#include <deque>
#include <memory>
#include <optional>

namespace OpenKneeboard {

class OpenXRNext;

class OpenXRGestureTracker final {
 public:
  OpenXRGestureTracker(
    XrInstance instance,
    XrSystemId systemId,
    XrSession session,
    const std::shared_ptr<OpenXRNext>& next,
    bool dataSourceExtensionEnabled = false);
  ~OpenXRGestureTracker();

  bool IsSupported() const noexcept { return mHandTrackingSupported; }

  void LocateHands(XrSpace locateSpace, XrTime displayTime);

  [[nodiscard]]
  std::optional<VRKneeboard::Pose> GetHandMountPose(
    VRFollowMode mode) const noexcept;

  [[nodiscard]]
  std::optional<VRKneeboard::ControllerInput> GetHandRayInput(
    uint8_t preferredHand) const noexcept;

  void PollLocatedGestures(
    const VRKneeboard::Pose& hmdPose,
    const std::optional<VRKneeboard::Layer>& activeLayer,
    const VRGestureSettings& settings);

 private:
  struct HandSample {
    DirectX::SimpleMath::Vector3 mLocalPos;
    std::chrono::steady_clock::time_point mTime;
  };

  struct HandState {
    XrHandTrackerEXT mTracker {XR_NULL_HANDLE};
    std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> mJoints {};
    bool mLocated {false};
    std::optional<VRKneeboard::Pose> mWristPose;
    std::deque<HandSample> mTrail;
    bool mIsPinching {false};
    bool mPinchMoved {false};
    DirectX::SimpleMath::Vector3 mPinchStartLocalPos {};
    std::chrono::steady_clock::time_point mPinchStart {};
    std::optional<std::chrono::steady_clock::time_point> mPalmHoldStart;
    std::chrono::steady_clock::time_point mLastActionTime {};
  };

  void DispatchAction(UserAction action);
  void ProcessHand(
    HandState& hand,
    const VRKneeboard::Pose& hmdPose,
    const VRGestureSettings& settings);

  std::shared_ptr<OpenXRNext> mOpenXR;
  bool mHandTrackingSupported {false};
  bool mLoggedFirstActiveHand {false};
  std::array<HandState, 2> mHands {};

  PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT {nullptr};
  PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT {nullptr};
  PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT {nullptr};
};

}// namespace OpenKneeboard
