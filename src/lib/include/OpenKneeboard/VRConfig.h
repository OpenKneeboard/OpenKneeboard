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

#include <cstdint>
#include <numbers>

#include "OpenXRMode.h"
#include "bitflags.h"

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
#include <nlohmann/json_fwd.hpp>
#endif

namespace OpenKneeboard {

#pragma pack(push)
struct VRRenderConfig {
  static constexpr uint16_t VERSION = 3;

  enum class Flags : uint32_t {
    // HEADLOCKED = 1 << 0,
    DISCARD_DEPTH_INFORMATION = 1 << 1,
    // PREFER_ROOMSCALE_POSITION = 1 << 2,
    GAZE_ZOOM = 1 << 3,
    FORCE_ZOOM = 1 << 4,
  };

  // Distances in meters, rotations in radians
  float mX = 0.15f, mFloorY = 0.6f, mEyeY = -0.7f, mZ = -0.4f;
  float mRX = -2 * std::numbers::pi_v<float> / 5,
        mRY = -std::numbers::pi_v<float> / 32, mRZ = 0.0f;
  float mHeight = 0.25f;
  float mZoomScale = 2.0f;

  // Increment every time binding is pressed
  uint64_t mRecenterCount = 0;

  float mGazeTargetVerticalScale = 1.0f;
  float mGazeTargetHorizontalScale = 1.0f;

  Flags mFlags = static_cast<Flags>(
    static_cast<uint32_t>(Flags::DISCARD_DEPTH_INFORMATION)
    | static_cast<uint32_t>(Flags::GAZE_ZOOM));
};
#pragma pack(pop)

struct VRConfig : public VRRenderConfig {
  bool mEnableSteamVR = true;
  OpenXRMode mOpenXRMode = OpenXRMode::CurrentUser;
};

template <>
constexpr bool is_bitflags_v<VRRenderConfig::Flags> = true;

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
// not using the macros because they use `x`, but we're
// dealing with coordinates so have an `x`. We'll also
// likely want custom functions anyway to deal with older
// configs when we add new fields.
void from_json(const nlohmann::json& j, VRConfig& vrc);
void to_json(nlohmann::json& j, const VRConfig& vrc);
#endif

}// namespace OpenKneeboard
