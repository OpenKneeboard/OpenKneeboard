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

#include "bitflags.h"

#include <cstdint>
#include <numbers>

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
#include <OpenKneeboard/json_fwd.h>
#endif

namespace OpenKneeboard {

struct VRLayerConfig {
  // Distances in meters, rotations in radians
  float mX = 0.15f, mEyeY = -0.7f, mZ = -0.4f;
  float mRX = -2 * std::numbers::pi_v<float> / 5,
        mRY = -std::numbers::pi_v<float> / 32, mRZ = 0.0f;
  float mWidth = 0.25f;
  float mHeight = 0.25f;

  constexpr auto operator<=>(const VRLayerConfig&) const noexcept = default;
};

struct VRRenderConfig {
  struct Quirks final {
    bool mOculusSDK_DiscardDepthInformation {true};
    bool mVarjo_OpenXR_D3D12_DoubleBuffer {true};
    bool mOpenXR_AlwaysUpdateSwapchain {false};
    constexpr auto operator<=>(const Quirks&) const noexcept = default;
  };
  Quirks mQuirks {};
  bool mEnableGazeInputFocus {true};
  bool mEnableGazeZoom {true};

  float mZoomScale = 2.0f;

  struct GazeTargetScale final {
    float mVertical {1.0f};
    float mHorizontal {1.0f};
    constexpr auto operator<=>(const GazeTargetScale&) const noexcept = default;
  };
  GazeTargetScale mGazeTargetScale {};

  struct Opacity final {
    float mNormal {1.0f};
    float mGaze {1.0f};
    constexpr auto operator<=>(const Opacity&) const noexcept = default;
  };
  Opacity mOpacity {};

  ///// Runtime-only settings (no JSON) ////

  bool mForceZoom {false};
  // Increment every time binding is pressed
  uint64_t mRecenterCount = 0;

  constexpr auto operator<=>(const VRRenderConfig&) const noexcept = default;
};
static_assert(std::is_standard_layout_v<VRRenderConfig>);

struct VRConfig : public VRRenderConfig {
  bool mEnableSteamVR = true;
  VRLayerConfig mPrimaryLayer;
  float mMaxWidth = 0.15f;
  float mMaxHeight = 0.25f;

  constexpr auto operator<=>(const VRConfig&) const noexcept = default;
};

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
OPENKNEEBOARD_DECLARE_SPARSE_JSON(VRConfig)
#endif

}// namespace OpenKneeboard
