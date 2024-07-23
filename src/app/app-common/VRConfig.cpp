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
#include <OpenKneeboard/VRConfig.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

VRPose VRPose::GetHorizontalMirror() const {
  auto ret = *this;
  ret.mX = -ret.mX;
  // Yaw
  ret.mRY = -ret.mRY;
  // Roll
  ret.mRZ = -ret.mRZ;
  return ret;
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(VRPose, mX, mEyeY, mZ, mRX, mRY, mRZ)

NLOHMANN_JSON_SERIALIZE_ENUM(
  VRRenderConfig::Quirks::Upscaling,
  {
    {VRRenderConfig::Quirks::Upscaling::Automatic, "Automatic"},
    {VRRenderConfig::Quirks::Upscaling::AlwaysOff, "AlwaysOff"},
    {VRRenderConfig::Quirks::Upscaling::AlwaysOn, "AlwaysOn"},
  });

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  VRRenderConfig::Quirks,
  mOculusSDK_DiscardDepthInformation,
  mOpenXR_AlwaysUpdateSwapchain,
  mOpenXR_Upscaling)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(GazeTargetScale, mVertical, mHorizontal);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(VROpacityConfig, mNormal, mGaze);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(VRRenderConfig, mQuirks, mEnableGazeInputFocus)

template <>
void from_json_postprocess<VRConfig>(const nlohmann::json& j, VRConfig& v) {
  from_json(j, static_cast<VRRenderConfig&>(v));
  from_json(j, v.mDeprecated.mPrimaryLayer);

  // Backwards compatibility
  if (j.contains("height")) {
    v.mDeprecated.mMaxHeight = j.at("height");
  }
  if (j.contains("width")) {
    v.mDeprecated.mMaxWidth = j.at("width");
  }
}

template <>
void to_json_postprocess<VRConfig>(
  nlohmann::json& j,
  const VRConfig& parent_v,
  const VRConfig& v) {
  to_json_with_default(
    j,
    static_cast<const VRRenderConfig&>(parent_v),
    static_cast<const VRRenderConfig&>(v));
  to_json_with_default(
    j, parent_v.mDeprecated.mPrimaryLayer, v.mDeprecated.mPrimaryLayer);
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  VRConfig::Deprecated,
  mMaxWidth,
  mMaxHeight,
  mEnableGazeZoom,
  mZoomScale,
  mGazeTargetScale,
  mOpacity)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(VRConfig, mEnableSteamVR)

}// namespace OpenKneeboard
