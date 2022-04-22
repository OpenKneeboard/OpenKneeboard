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
#include <OpenKneeboard/VRConfig.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {
void from_json(const nlohmann::json& j, VRConfig& vrc) {
  vrc.mX = j.at("x");
  vrc.mEyeY = j.at("eyeY");
  vrc.mFloorY = j.at("floorY");
  vrc.mZ = j.at("z");
  vrc.mRX = j.at("rx");
  vrc.mRY = j.at("ry");
  vrc.mRZ = j.at("rz");
  vrc.mHeight = j.at("height");
  vrc.mZoomScale = j.at("zoomScale");

  if (j.contains("gazeTargetScale")) {
    auto scale = j.at("gazeTargetScale");
    vrc.mGazeTargetHorizontalScale = scale.at("horizontal");
    vrc.mGazeTargetVerticalScale = scale.at("vertical");
  }

  if (j.contains("discardOculusDepthInformation")) {
    if (j.at("discardOculusDepthInformation").get<bool>()) {
      vrc.mFlags |= VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION;
    } else {
      vrc.mFlags &= ~VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION;
    }
  }

  if (j.contains("enableGazeZoom")) {
    if (j.at("enableGazeZoom").get<bool>()) {
      vrc.mFlags |= VRRenderConfig::Flags::GAZE_ZOOM;
    } else {
      vrc.mFlags &= ~VRRenderConfig::Flags::GAZE_ZOOM;
    }
  }

  if (j.contains("enableSteamVR")) {
    vrc.mEnableSteamVR = j.at("enableSteamVR").get<bool>();
  }

  if (j.contains("openXRMode")) {
    vrc.mOpenXRMode = j.at("openXRMode").get<OpenXRMode>();
  }
}

void to_json(nlohmann::json& j, const VRConfig& vrc) {
  j = {
    {"x", vrc.mX},
    {"eyeY", vrc.mEyeY},
    {"floorY", vrc.mFloorY},
    {"z", vrc.mZ},
    {"rx", vrc.mRX},
    {"ry", vrc.mRY},
    {"rz", vrc.mRZ},
    {"height", vrc.mHeight},
    {"zoomScale", vrc.mZoomScale},
    {"discardOculusDepthInformation",
     static_cast<bool>(
       vrc.mFlags & VRRenderConfig::Flags::DISCARD_DEPTH_INFORMATION)},
    {
      "gazeTargetScale",
      {
        {"horizontal", vrc.mGazeTargetHorizontalScale},
        {"vertical", vrc.mGazeTargetVerticalScale},
      },
    },
    {"enableGazeZoom",
     static_cast<bool>(vrc.mFlags & VRRenderConfig::Flags::GAZE_ZOOM)},
    {"enableSteamVR", vrc.mEnableSteamVR},
    {"openXRMode", vrc.mOpenXRMode},
  };
}
}// namespace OpenKneeboard
