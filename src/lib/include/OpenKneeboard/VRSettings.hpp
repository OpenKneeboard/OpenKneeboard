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
#pragma once

#include "bitflags.hpp"

#include <OpenKneeboard/Geometry2D.hpp>

#include <cstdint>
#include <numbers>

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
struct VRRenderSettings {
  struct Quirks final {
    bool mOculusSDK_DiscardDepthInformation {false};

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

  ///// Runtime-only settings (no JSON) ////

  bool mForceZoom {false};
  // Increment every time binding is pressed
  uint64_t mRecenterCount = 0;

  constexpr bool operator==(const VRRenderSettings&) const noexcept = default;
};
static_assert(std::is_standard_layout_v<VRRenderSettings>);

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
