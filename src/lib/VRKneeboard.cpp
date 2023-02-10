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
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/RayIntersectsRect.h>
#include <OpenKneeboard/VRKneeboard.h>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

VRKneeboard::Pose VRKneeboard::GetKneeboardPose(
  const VRRenderConfig& vr,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
  const auto& vrl = layer.mVR;
  this->MaybeRecenter(vr, hmdPose);
  auto matrix = Matrix::CreateRotationX(vrl.mRX)
    * Matrix::CreateRotationY(vrl.mRY) * Matrix::CreateRotationZ(vrl.mRZ)
    * Matrix::CreateTranslation({
      vrl.mX,
      vrl.mEyeY + *mEyeHeight,
      vrl.mZ,
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
      || (isLookingAtKneeboard && config.mVR.mEnableGazeZoom)
    ? sizes.mZoomedSize
    : sizes.mNormalSize;
}

VRKneeboard::Sizes VRKneeboard::GetSizes(
  const VRRenderConfig& vrc,
  const SHM::LayerConfig& layer) const {
  const auto& vr = layer.mVR;
  const auto aspectRatio = float(layer.mImageWidth) / layer.mImageHeight;
  const auto virtualHeight = vr.mHeight;
  const auto virtualWidth = aspectRatio * vr.mHeight;

  return {
    .mNormalSize = {virtualWidth, virtualHeight},
    .mZoomedSize
    = {virtualWidth * vrc.mZoomScale, virtualHeight * vrc.mZoomScale},
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

SHM::Snapshot VRKneeboard::MaybeGetSnapshot(SHM::ConsumerKind consumerKind) {
  // Cache, so we don't need to fight for the spinlock if there's
  // no new frames
  if (!mSHM) {
    return {};
  }

  if (
    mSnapshot.IsValid()
    && mSHM.GetRenderCacheKey() == mSnapshot.GetRenderCacheKey()
    && consumerKind == mConsumerKind) {
    return mSnapshot;
  }

  const std::unique_lock lock(mSHM);
  mSnapshot = mSHM.MaybeGet(consumerKind);
  mConsumerKind = consumerKind;
  return mSnapshot;
}

bool VRKneeboard::HaveSHM() const {
  return static_cast<bool>(mSHM);
}

VRKneeboard::RenderParameters VRKneeboard::GetRenderParameters(
  const SHM::Snapshot& snapshot,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
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
    .mKneeboardOpacity = isLookingAtKneeboard ? config.mVR.mOpacity.mGaze
                                              : config.mVR.mOpacity.mNormal,
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
    config.mVR.mGazeTargetScale.mHorizontal < 0.1
    || config.mVR.mGazeTargetScale.mVertical < 0.1) {
    return false;
  }

  const auto sizes = this->GetSizes(config.mVR, layer);
  auto currentSize
    = isLookingAtKneeboard ? sizes.mZoomedSize : sizes.mNormalSize;

  currentSize.x *= config.mVR.mGazeTargetScale.mHorizontal;
  currentSize.y *= config.mVR.mGazeTargetScale.mVertical;

  isLookingAtKneeboard = RayIntersectsRect(
    hmdPose.mPosition,
    hmdPose.mOrientation,
    kneeboardPose.mPosition,
    kneeboardPose.mOrientation,
    currentSize);

  if (
    isLookingAtKneeboard && config.mGlobalInputLayerID != layer.mLayerID
    && config.mVR.mEnableGazeInputFocus) {
    GameEvent {
      GameEvent::EVT_SET_INPUT_FOCUS,
      std::to_string(layer.mLayerID),
    }
      .Send();
  }

  return isLookingAtKneeboard;
}

}// namespace OpenKneeboard
