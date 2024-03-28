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
#include <OpenKneeboard/RayIntersectsRect.h>
#include <OpenKneeboard/SHM/ActiveConsumers.h>
#include <OpenKneeboard/VRKneeboard.h>

#include <ranges>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

VRKneeboard::Pose VRKneeboard::GetKneeboardPose(
  const VRRenderConfig& vr,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
  const auto& pose = layer.mVR.mPose;
  this->MaybeRecenter(vr, hmdPose);
  auto matrix = Matrix::CreateRotationX(pose.mRX)
    * Matrix::CreateRotationY(pose.mRY) * Matrix::CreateRotationZ(pose.mRZ)
    * Matrix::CreateTranslation({
      pose.mX,
      pose.mEyeY + *mEyeHeight,
      pose.mZ,
    })
    * mRecenter;

  return {
    .mPosition = matrix.Translation(),
    .mOrientation = Quaternion::CreateFromRotationMatrix(matrix),
  };
}

Vector2 VRKneeboard::GetKneeboardSize(
  const SHM::Config& config,
  const SHM::LayerConfig& layer,
  bool isLookingAtKneeboard) {
  const auto sizes = this->GetSizes(config.mVR, layer);

  return config.mVR.mForceZoom
      || (isLookingAtKneeboard && layer.mVR.mEnableGazeZoom)
    ? sizes.mZoomedSize
    : sizes.mNormalSize;
}

VRKneeboard::Sizes VRKneeboard::GetSizes(
  const VRRenderConfig& vrc,
  const SHM::LayerConfig& layer) const {
  const auto& physicalSize = layer.mVR.mPhysicalSize;
  const auto virtualWidth = physicalSize.mWidth;
  const auto virtualHeight = physicalSize.mHeight;

  return {
    .mNormalSize = {virtualWidth, virtualHeight},
    .mZoomedSize
    = {virtualWidth * layer.mVR.mZoomScale, virtualHeight * layer.mVR.mZoomScale},
  };
}

void VRKneeboard::MaybeRecenter(const VRRenderConfig& vr, const Pose& hmdPose) {
  if (vr.mRecenterCount == mRecenterCount) {
    return;
  }
  this->Recenter(vr, hmdPose);
}

void VRKneeboard::Recenter(const VRRenderConfig& vr, const Pose& hmdPose) {
  auto pos = hmdPose.mPosition;
  mEyeHeight = {pos.y};
  pos.y = 0;

  // We're only going to respect ry (yaw) as we want the new
  // center to remain gravity-aligned

  auto quat = hmdPose.mOrientation;

  // clang-format off
    mRecenter =
      Matrix::CreateRotationY(quat.ToEuler().y) 
      * Matrix::CreateTranslation({pos.x, pos.y, pos.z});
  // clang-format on

  mRecenterCount = vr.mRecenterCount;
}

std::vector<VRKneeboard::Layer> VRKneeboard::GetLayers(
  const SHM::Snapshot& snapshot,
  const Pose& hmdPose) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }

  const auto totalLayers = snapshot.GetLayerCount();

  std::vector<Layer> ret;
  ret.reserve(totalLayers);
  for (uint32_t layerIndex = 0; layerIndex < totalLayers; ++layerIndex) {
    const auto layerConfig = snapshot.GetLayerConfig(layerIndex);
    if (!layerConfig->mVREnabled) {
      continue;
    }

    ret.push_back(Layer {
      layerConfig, GetRenderParameters(snapshot, *layerConfig, hmdPose)});
  }

  const auto config = snapshot.GetConfig();
  if (config.mVR.mEnableGazeInputFocus) {
    const auto activeLayerID = config.mGlobalInputLayerID;

    for (const auto& [layerConfig, renderParams]:
         std::ranges::reverse_view(ret)) {
      if (
        renderParams.mIsLookingAtKneeboard
        && layerConfig->mLayerID != activeLayerID) {
        SHM::ActiveConsumers::SetActiveInGameViewID(layerConfig->mLayerID);
        break;
      }
    }
  }

  return ret;
}

VRKneeboard::RenderParameters VRKneeboard::GetRenderParameters(
  const SHM::Snapshot& snapshot,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose) {
  auto config = snapshot.GetConfig();
  const auto kneeboardPose = this->GetKneeboardPose(config.mVR, layer, hmdPose);
  const auto isLookingAtKneeboard
    = this->IsLookingAtKneeboard(config, layer, hmdPose, kneeboardPose);

  auto cacheKey = snapshot.GetRenderCacheKey();
  if (isLookingAtKneeboard) {
    cacheKey |= 1;
  } else {
    cacheKey &= ~static_cast<size_t>(1);
  }

  return {
    .mKneeboardPose = kneeboardPose,
    .mKneeboardSize
    = this->GetKneeboardSize(config, layer, isLookingAtKneeboard),
    .mKneeboardOpacity = isLookingAtKneeboard ? layer.mVR.mOpacity.mGaze
                                              : layer.mVR.mOpacity.mNormal,
    .mCacheKey = cacheKey,
    .mIsLookingAtKneeboard = isLookingAtKneeboard,
  };
}

bool VRKneeboard::IsLookingAtKneeboard(
  const SHM::Config& config,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose,
  const Pose& kneeboardPose) {
  auto& isLookingAtKneeboard = mIsLookingAtKneeboard[layer.mLayerID];

  if (
    layer.mVR.mGazeTargetScale.mHorizontal < 0.1
    || layer.mVR.mGazeTargetScale.mVertical < 0.1) {
    return false;
  }

  const auto sizes = this->GetSizes(config.mVR, layer);
  auto currentSize
    = isLookingAtKneeboard ? sizes.mZoomedSize : sizes.mNormalSize;

  currentSize.x *= layer.mVR.mGazeTargetScale.mHorizontal;
  currentSize.y *= layer.mVR.mGazeTargetScale.mVertical;

  isLookingAtKneeboard = RayIntersectsRect(
    hmdPose.mPosition,
    hmdPose.mOrientation,
    kneeboardPose.mPosition,
    kneeboardPose.mOrientation,
    currentSize);

  return isLookingAtKneeboard;
}

}// namespace OpenKneeboard
