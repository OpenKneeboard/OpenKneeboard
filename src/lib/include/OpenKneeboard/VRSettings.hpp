// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "bitflags.hpp"

#include <OpenKneeboard/Geometry2D.hpp>

#include <cstdint>
#include <numbers>
#include <type_traits>

namespace OpenKneeboard {

/** Position and orientation.
 *
 * This is what's stored in the config file, so intended to be
 * semi-human-editable: distances in meters, rotations in radians.
 */
struct VRPose {
  float mX = 0.15f, mEyeY = -0.7f, mZ = -0.4f;
  float mRX = -2 * std::numbers::pi_v<float> / 5,
        mRY = -std::numbers::pi_v<float> / 32, mRZ = 0.0f;

  VRPose GetHorizontalMirror() const;

  constexpr bool operator==(const VRPose&) const noexcept = default;
};

/** If gaze zoom is enabled, how close you need to be looking for zoom to
 * activate */
struct GazeTargetScale {
  float mVertical {1.0f};
  float mHorizontal {1.0f};
  constexpr bool operator==(const GazeTargetScale&) const noexcept = default;
};

struct VROpacitySettings {
  float mNormal {1.0f};
  float mGaze {1.0f};
  constexpr bool operator==(const VROpacitySettings&) const noexcept = default;
};

/** VR settings that apply to every view/layer.
 *
 * Per-view settings are in `ViewVRSettings`
 *
 * This ends up in the SHM; it is extended by `VRSettings` for
 * values that are stored in the config file but need further processing before
 * being put in SHM.
 */
enum class VRFollowMode : uint8_t {
  WorldLocked = 0,
  HeadLocked = 1,
  HeadFollowSmooth = 2,
  HeadFollowLazy = 3,
  LeftHand = 4,
  RightHand = 5,
};

struct VRHeadFollowSettings {
  VRFollowMode mMode {VRFollowMode::HeadFollowLazy};
  float mDeadzoneAngleDeg {18.0f};
  float mDeadzoneDistanceM {0.08f};
  float mFollowSpeed {7.5f};
  bool mLockRoll {true};
  bool mLockPitch {false};
  bool mPinToHand {false};
  uint8_t mPinnedHand {0};
  float mHeadOffsetX {0.10f};
  float mHeadOffsetY {-0.06f};
  float mHeadOffsetZ {-0.58f};
  float mHeadPitchDeg {-6.0f};
  float mHeadYawDeg {-10.0f};
  float mHandOffsetX {0.0f};
  float mHandOffsetY {0.18f};
  float mHandOffsetZ {0.0f};
  float mHandPitchDeg {0.0f};
  float mHandForwardX {0.0f};
  float mHandForwardY {0.0f};
  float mHandForwardZ {1.0f};
  float mWristTiltOffset {0.0f};
  constexpr bool operator==(const VRHeadFollowSettings&) const noexcept = default;
};

[[nodiscard]] constexpr VRFollowMode GetEffectiveFollowMode(
  const VRHeadFollowSettings& hf) noexcept {
  if (hf.mPinToHand) {
    return (hf.mPinnedHand == 1) ? VRFollowMode::RightHand
                                 : VRFollowMode::LeftHand;
  }
  return hf.mMode;
}

struct VRGestureSettings {
  bool mEnabled {true};
  bool mAllowPinchReposition {true};
  float mPinchThresholdM {0.025f};
  float mSwipeMinDistanceM {0.14f};
  uint32_t mCooldownMs {450};
  constexpr bool operator==(const VRGestureSettings&) const noexcept = default;
};

struct VRRenderSettings {
  struct Quirks final {
    enum class Upscaling {
      Automatic,// Varjo-only
      AlwaysOff,
      AlwaysOn,
    };
    Upscaling mOpenXR_Upscaling {Upscaling::Automatic};

    constexpr bool operator==(const Quirks&) const noexcept = default;
  };
  Quirks mQuirks {};
  bool mEnableGazeInputFocus {true};
  bool mEditModeEnabled {false};
  bool mAllowControllerManipulation {true};
  VRHeadFollowSettings mHeadFollow {};
  VRGestureSettings mGestures {};

  ///// Runtime-only settings (no JSON) ////

  bool mForceZoom {false};
  // Increment every time binding is pressed
  uint64_t mRecenterCount = 0;

  constexpr bool operator==(const VRRenderSettings&) const noexcept = default;
};
static_assert(std::is_standard_layout_v<VRRenderSettings>);
static_assert(sizeof(VRHeadFollowSettings) == 72);
static_assert(sizeof(VRGestureSettings) == 16);
static_assert(sizeof(VRRenderSettings) == 112);

/// VR settings, including ones that are not directly sent in SHM
struct VRSettings : public VRRenderSettings {
  bool mEnableSteamVR = true;

  // replaced with 'ViewSettings' and `IndependentViewVRSettings` in v1.7
  struct Deprecated {
    VRPose mPrimaryLayer {};
    float mMaxWidth = 0.15f;
    float mMaxHeight = 0.25f;

    bool mEnableGazeZoom {true};
    float mZoomScale = 2.0f;
    GazeTargetScale mGazeTargetScale {};
    VROpacitySettings mOpacity {};

    constexpr bool operator==(const Deprecated&) const noexcept = default;
  };

  Deprecated mDeprecated;

  constexpr bool operator==(const VRSettings&) const noexcept = default;
};

}// namespace OpenKneeboard
