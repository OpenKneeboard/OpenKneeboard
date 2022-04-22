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

#include <DirectXTK/SimpleMath.h>
#include <OpenKneeboard/RayIntersectsRect.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRConfig.h>

namespace OpenKneeboard {

template <class TDisplayTime>
class VRKneeboard {
 protected:
  using Matrix = DirectX::SimpleMath::Matrix;
  using Quaternion = DirectX::SimpleMath::Quaternion;
  using Vector2 = DirectX::SimpleMath::Vector2;
  using Vector3 = DirectX::SimpleMath::Vector3;

  struct Pose {
    Vector3 mPosition {};
    Quaternion mOrientation {};
  };

  enum class YOrigin {
    FLOOR_LEVEL,
    EYE_LEVEL,
  };

  virtual Pose GetHMDPose(TDisplayTime) = 0;
  virtual YOrigin GetYOrigin() = 0;

  Pose GetKneeboardPose(const VRRenderConfig& vr, TDisplayTime displayTime) {
    this->Recenter(vr, displayTime);
    // clang-format off
    auto matrix =
      Matrix::CreateRotationX(vr.mRX)
      * Matrix::CreateRotationY(vr.mRY)
      * Matrix::CreateRotationZ(vr.mRZ)
      * Matrix::CreateTranslation({
        vr.mX,
        this->GetYOrigin() == YOrigin::EYE_LEVEL ? vr.mEyeY : vr.mFloorY, vr.mZ})
      * mRecenter;
    // clang-format on
    return {
      .mPosition = matrix.Translation(),
      .mOrientation = Quaternion::CreateFromRotationMatrix(matrix),
    };
  }

  Vector2 GetKneeboardSize(
    const SHM::Config& config,
    const Pose& kneeboardPose,
    TDisplayTime displayTime) {
    const auto sizes = this->GetSizes(config);
    if (!this->IsGazeEnabled(config.vr)) {
      return sizes.mNormalSize;
    }
    auto hmdPose = this->GetHMDPose(displayTime);
    return this->IsLookingAtKneeboard(config, hmdPose, kneeboardPose)
      ? sizes.mZoomedSize
      : sizes.mNormalSize;
  }

 private:
  uint64_t mRecenterCount = 0;
  Matrix mRecenter = Matrix::Identity;

  bool IsGazeEnabled(const VRRenderConfig& vr) {
    if (vr.mFlags & VRRenderConfig::Flags::FORCE_ZOOM) {
      return false;
    }
    if (!(vr.mFlags & VRRenderConfig::Flags::GAZE_ZOOM)) {
      return false;
    }

    if (
      vr.mZoomScale < 1.1 || vr.mGazeTargetHorizontalScale < 0.1
      || vr.mGazeTargetVerticalScale < 0.1) {
      return false;
    }

    return true;
  }

  struct {
    Vector2 mNormalSize;
    Vector2 mZoomedSize;
  } GetSizes(const SHM::Config& config) const {
    const auto& vr = config.vr;
    const auto aspectRatio = float(config.imageWidth) / config.imageHeight;
    const auto virtualHeight = vr.mHeight;
    const auto virtualWidth = aspectRatio * vr.mHeight;

    return {
      .mNormalSize = {virtualWidth, virtualHeight},
      .mZoomedSize
      = {virtualWidth * vr.mZoomScale, virtualHeight * vr.mZoomScale},
    };
  }

  void Recenter(const VRRenderConfig& vr, TDisplayTime displayTime) {
    if (vr.mRecenterCount == mRecenterCount) {
      return;
    }

    auto hmd = this->GetHMDPose(displayTime);
    auto& pos = hmd.mPosition;
    pos.y = 0;// don't adjust floor level

    // We're only going to respect ry (yaw) as we want the new
    // center to remain gravity-aligned

    auto quat = hmd.mOrientation;

    // clang-format off
		mRecenter =
			Matrix::CreateRotationY(quat.ToEuler().y) 
			* Matrix::CreateTranslation({pos.x, pos.y, pos.z});
    // clang-format on

    mRecenterCount = vr.mRecenterCount;
  }

  bool IsLookingAtKneeboard(
    const SHM::Config& config,
    const Pose& hmdPose,
    const Pose& kneeboardPose) {
    static bool sIsLookingAtKneeboard = false;
    if (!this->IsGazeEnabled(config.vr)) {
      return false;
    }

    const auto sizes = this->GetSizes(config);
    const auto currentSize
      = sIsLookingAtKneeboard ? sizes.mZoomedSize : sizes.mNormalSize;

    sIsLookingAtKneeboard = RayIntersectsRect(
      hmdPose.mPosition,
      hmdPose.mOrientation,
      kneeboardPose.mPosition,
      kneeboardPose.mOrientation,
      currentSize);

    return sIsLookingAtKneeboard;
  }
};

}// namespace OpenKneeboard