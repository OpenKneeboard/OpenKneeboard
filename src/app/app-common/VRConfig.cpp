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
  auto& l = vrc.mPrimaryLayer;
  l.mX = j.at("x");
  l.mEyeY = j.at("eyeY");
  l.mFloorY = j.at("floorY");
  l.mZ = j.at("z");
  l.mRX = j.at("rx");
  l.mRY = j.at("ry");
  l.mRZ = j.at("rz");
  l.mHeight = j.at("height");
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

  if (j.contains("invertOpenXRYPosition")) {
    if (j.at("invertOpenXRYPosition").get<bool>()) {
      vrc.mFlags |= VRRenderConfig::Flags::INVERT_OPENXR_Y_POSITION;
    } else {
      vrc.mFlags &= ~VRRenderConfig::Flags::INVERT_OPENXR_Y_POSITION;
    }
  }

  if (j.contains("opacity")) {
    auto opacity = j.at("opacity");
    vrc.mNormalOpacity = opacity.at("normal");
    vrc.mGazeOpacity = opacity.at("gaze");
  }

  if (j.contains("dualKneeboards")) {
    auto dualKneeboards = j.at("dualKneeboards");
    if (dualKneeboards.at("enableGazeInputFocus").get<bool>()) {
      vrc.mFlags |= VRConfig::Flags::GAZE_INPUT_FOCUS;
    } else {
      vrc.mFlags &= ~VRConfig::Flags::GAZE_INPUT_FOCUS;
    }
  }
}

void to_json(nlohmann::json& j, const VRConfig& vrc) {
  const auto& l = vrc.mPrimaryLayer;
  j = {
    {"x", l.mX},
    {"eyeY", l.mEyeY},
    {"floorY", l.mFloorY},
    {"z", l.mZ},
    {"rx", l.mRX},
    {"ry", l.mRY},
    {"rz", l.mRZ},
    {"height", l.mHeight},
    {"zoomScale", vrc.mZoomScale},
    {"invertOpenXRYPosition",
     static_cast<bool>(
       vrc.mFlags & VRRenderConfig::Flags::INVERT_OPENXR_Y_POSITION)},
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
    {
      "opacity",
      {
        {"normal", vrc.mNormalOpacity},
        {"gaze", vrc.mGazeOpacity},
      },
    },
    {
      "dualKneeboards",
      {
        {
          "enableGazeInputFocus",
          static_cast<bool>(vrc.mFlags & VRConfig::Flags::GAZE_INPUT_FOCUS),
        },
      },
    },
  };
}
}// namespace OpenKneeboard
