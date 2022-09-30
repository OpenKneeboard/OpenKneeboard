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
#include <OpenKneeboard/json.h>

namespace OpenKneeboard {

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  VRLayerConfig,
  mX,
  mFloorY,
  mEyeY,
  mZ,
  mRX,
  mRY,
  mRZ,
  mWidth,
  mHeight)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  VRRenderConfig::Quirks,
  mOculusSDK_DiscardDepthInformation,
  mVarjo_OpenXR_InvertYPosition,
  mVarjo_OpenXR_D3D12_DoubleBuffer)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  VRRenderConfig::GazeTargetScale,
  mVertical,
  mHorizontal);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(VRRenderConfig::Opacity, mNormal, mGaze);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  VRRenderConfig,
  mQuirks,
  mEnableGazeInputFocus,
  mEnableGazeZoom,
  mZoomScale,
  mGazeTargetScale,
  mOpacity)

template <>
void from_json_postprocess<VRConfig>(const nlohmann::json& j, VRConfig& v) {
  from_json(j, static_cast<VRRenderConfig&>(v));
  from_json(j, v.mPrimaryLayer);

  // Backwards compatibility
  if (j.contains("height")) {
    v.mMaxHeight = j.at("height");
  }
  if (j.contains("width")) {
    v.mMaxWidth = j.at("width");
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
  to_json_with_default(j, parent_v.mPrimaryLayer, v.mPrimaryLayer);
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  VRConfig,
  mEnableSteamVR,
  mOpenXRMode,
  mMaxWidth,
  mMaxHeight)

}// namespace OpenKneeboard
