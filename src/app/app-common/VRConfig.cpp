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
  vrc.x = j.at("x");
  vrc.eyeY = j.at("eyeY");
  vrc.floorY = j.at("floorY");
  vrc.z = j.at("z");
  vrc.rx = j.at("rx");
  vrc.ry = j.at("ry");
  vrc.rz = j.at("rz");
  vrc.height = j.at("height");
  vrc.zoomScale = j.at("zoomScale");

  if (j.contains("gazeTargetScale")) {
    auto scale = j.at("gazeTargetScale");
    vrc.gazeTargetHorizontalScale = scale.at("horizontal");
    vrc.gazeTargetVerticalScale = scale.at("vertical");
  }

  if (j.contains("headLocked")) {
    if (j.at("headLocked").get<bool>()) {
      vrc.flags |= VRConfig::Flags::HEADLOCKED;
    } else {
      vrc.flags &= ~VRConfig::Flags::HEADLOCKED;
    }
  }

  if (j.contains("discardOculusDepthInformation")) {
    if (j.at("discardOculusDepthInformation").get<bool>()) {
      vrc.flags |= VRConfig::Flags::DISCARD_DEPTH_INFORMATION;
    } else {
      vrc.flags &= ~VRConfig::Flags::DISCARD_DEPTH_INFORMATION;
    }
  }

  if (j.contains("enableGazeZoom")) {
    if (j.at("enableGazeZoom").get<bool>()) {
      vrc.flags |= VRConfig::Flags::GAZE_ZOOM;
    } else {
      vrc.flags &= ~VRConfig::Flags::GAZE_ZOOM;
    }
  }
}

void to_json(nlohmann::json& j, const VRConfig& vrc) {
  j = {
    {"x", vrc.x},
    {"eyeY", vrc.eyeY},
    {"floorY", vrc.floorY},
    {"z", vrc.z},
    {"rx", vrc.rx},
    {"ry", vrc.ry},
    {"rz", vrc.rz},
    {"height", vrc.height},
    {"zoomScale", vrc.zoomScale},
    {"headLocked", static_cast<bool>(vrc.flags & VRConfig::Flags::HEADLOCKED)},
    {"discardOculusDepthInformation",
     static_cast<bool>(vrc.flags & VRConfig::Flags::DISCARD_DEPTH_INFORMATION)},
    {
      "gazeTargetScale",
      {
        {"horizontal", vrc.gazeTargetHorizontalScale},
        {"vertical", vrc.gazeTargetVerticalScale},
      },
    },
    {"enableGazeZoom",
     static_cast<bool>(vrc.flags & VRConfig::Flags::GAZE_ZOOM)},
  };
}
}// namespace OpenKneeboard
