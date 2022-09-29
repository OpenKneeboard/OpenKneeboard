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

namespace {
using Flags = VRRenderConfig::Flags;
}

void SetFlagFromBool(
  const nlohmann::json& container,
  std::string_view key,
  VRConfig& vrc,
  const Flags flag,
  bool defaultValue) {
  const bool value = container.value<bool>(key, defaultValue);
  if (value) {
    vrc.mFlags |= flag;
  } else {
    vrc.mFlags &= ~flag;
  }
}

void SetQuirkFromBool(
  const nlohmann::json& container,
  std::string_view key,
  VRConfig& vrc,
  const Flags flag) {
  SetFlagFromBool(container, key, vrc, flag, /* default = */ true);
}

void from_json(const nlohmann::json& j, VRConfig& vrc) {
  auto& l = vrc.mPrimaryLayer;
  l.mX = j.at("x");
  l.mEyeY = j.at("eyeY");
  l.mFloorY = j.at("floorY");
  l.mZ = j.at("z");
  l.mRX = j.at("rx");
  l.mRY = j.at("ry");
  l.mRZ = j.at("rz");
  vrc.mMaxHeight = j.at("height");
  vrc.mZoomScale = j.at("zoomScale");
  if (j.contains("width")) {
    vrc.mMaxWidth = j.at("width");
  }

  if (j.contains("gazeTargetScale")) {
    auto scale = j.at("gazeTargetScale");
    vrc.mGazeTargetHorizontalScale = scale.at("horizontal");
    vrc.mGazeTargetVerticalScale = scale.at("vertical");
  }

  if (j.contains("quirks")) {
    auto& q = j.at("quirks");
    SetQuirkFromBool(
      q,
      "OculusSDK_DiscardDepthInformation",
      vrc,
      Flags::QUIRK_OCULUSSDK_DISCARD_DEPTH_INFORMATION);
    SetQuirkFromBool(
      q,
      "Varjo_OpenXR_D3D12_DoubleBuffer",
      vrc,
      Flags::QUIRK_VARJO_OPENXR_D3D12_DOUBLE_BUFFER);
    SetQuirkFromBool(
      q,
      "Varjo_OpenXR_InvertYPosition",
      vrc,
      Flags::QUIRK_VARJO_OPENXR_INVERT_Y_POSITION);
  }

  if (j.contains("enableGazeZoom")) {
    if (j.at("enableGazeZoom").get<bool>()) {
      vrc.mFlags |= Flags::GAZE_ZOOM;
    } else {
      vrc.mFlags &= ~Flags::GAZE_ZOOM;
    }
  }

  if (j.contains("enableSteamVR")) {
    vrc.mEnableSteamVR = j.at("enableSteamVR").get<bool>();
  }

  if (j.contains("openXRMode")) {
    vrc.mOpenXRMode = j.at("openXRMode").get<OpenXRMode>();
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
    {"width", vrc.mMaxWidth},
    {"height", vrc.mMaxHeight},
    {"zoomScale", vrc.mZoomScale},
    {
      "quirks",
      {
        {
          "OculusSDK_DiscardDepthInformation",
          static_cast<bool>(
            vrc.mFlags & Flags::QUIRK_OCULUSSDK_DISCARD_DEPTH_INFORMATION),
        },
        {
          "Varjo_OpenXR_D3D12_DoubleBuffer",
          static_cast<bool>(
            vrc.mFlags & Flags::QUIRK_VARJO_OPENXR_D3D12_DOUBLE_BUFFER),
        },
        {
          "Varjo_OpenXR_InvertYPosition",
          static_cast<bool>(
            vrc.mFlags & Flags::QUIRK_VARJO_OPENXR_INVERT_Y_POSITION),
        },
      },
    },
    {
      "gazeTargetScale",
      {
        {"horizontal", vrc.mGazeTargetHorizontalScale},
        {"vertical", vrc.mGazeTargetVerticalScale},
      },
    },
    {"enableGazeZoom", static_cast<bool>(vrc.mFlags & Flags::GAZE_ZOOM)},
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
