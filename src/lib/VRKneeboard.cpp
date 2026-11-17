// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/RayIntersectsRect.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/VRKneeboard.hpp>

#include <algorithm>
#include <cmath>
#include <ranges>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

namespace {
Matrix MakeHorizonAlignedAnchor(
  const VRKneeboard::Pose& hmdPose,
  const VRHeadFollowSettings& follow) {
  if (!follow.mLockRoll && !follow.mLockPitch) {
    return Matrix::CreateFromQuaternion(hmdPose.mOrientation)
      * Matrix::CreateTranslation(hmdPose.mPosition);
  }
  const auto forward = Vector3::Transform(Vector3::Forward, hmdPose.mOrientation);
  const float yaw = std::atan2(-forward.x, -forward.z);
  const float horiz = std::hypot(forward.x, forward.z);
  const float pitch = follow.mLockPitch
    ? 0.0f
    : std::atan2(forward.y, (std::max)(1e-5f, horiz));

  const auto q = Quaternion::CreateFromYawPitchRoll(yaw, pitch, 0.0f);
  return Matrix::CreateFromQuaternion(q)
    * Matrix::CreateTranslation(hmdPose.mPosition);
}

float QuaternionAngleDeg(const Quaternion& a, const Quaternion& b) {
  const float dot = std::clamp(std::abs(a.Dot(b)), 0.0f, 1.0f);
  return DirectX::XMConvertToDegrees(2.0f * std::acos(dot));
}
}// namespace

VRKneeboard::Pose VRKneeboard::GetKneeboardPose(
  const VRRenderSettings& vr,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose,
  const float dtSeconds) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
  const auto& pose = layer.mVR.mPose;
  this->MaybeRecenter(vr, hmdPose);
  const auto localMatrix = Matrix::CreateRotationX(pose.mRX)
    * Matrix::CreateRotationY(pose.mRY) * Matrix::CreateRotationZ(pose.mRZ)
    * Matrix::CreateTranslation({
      pose.mX,
      pose.mEyeY
        + (vr.mHeadFollow.mMode == VRFollowMode::WorldLocked ? *mEyeHeight : 0.0f),
      pose.mZ,
    });

  if (vr.mHeadFollow.mMode == VRFollowMode::WorldLocked) {
    const auto matrix = localMatrix * mRecenter;
    return {
      .mPosition = matrix.Translation(),
      .mOrientation = Quaternion::CreateFromRotationMatrix(matrix),
    };
  }

  const auto anchor = MakeHorizonAlignedAnchor(hmdPose, vr.mHeadFollow);
  const auto targetMatrix = localMatrix * anchor;
  const Pose targetPose {
    .mPosition = targetMatrix.Translation(),
    .mOrientation = Quaternion::CreateFromRotationMatrix(targetMatrix),
  };

  auto& state = mFollowStates[layer.mLayerID];
  if (!state.mInitialized || vr.mHeadFollow.mMode == VRFollowMode::HeadLocked) {
    state.mInitialized = true;
    state.mCurrentPose = targetPose;
    state.mAnchorHmdPosition = hmdPose.mPosition;
    return targetPose;
  }

  const float clampedDt = std::clamp(dtSeconds, 0.0f, 0.1f);
  const float alpha = 1.0f - std::exp(-vr.mHeadFollow.mFollowSpeed * clampedDt);

  if (vr.mHeadFollow.mMode == VRFollowMode::HeadFollowSmooth) {
    state.mCurrentPose.mPosition = Vector3::Lerp(
      state.mCurrentPose.mPosition, targetPose.mPosition, alpha);
    state.mCurrentPose.mOrientation = Quaternion::Slerp(
      state.mCurrentPose.mOrientation, targetPose.mOrientation, alpha);
    state.mAnchorHmdPosition = hmdPose.mPosition;
    return state.mCurrentPose;
  }

  const float angErrDeg = QuaternionAngleDeg(
    state.mCurrentPose.mOrientation, targetPose.mOrientation);
  const float hmdMoveM = Vector3::Distance(
    hmdPose.mPosition, state.mAnchorHmdPosition);

  const bool outside = angErrDeg > vr.mHeadFollow.mDeadzoneAngleDeg
    || hmdMoveM > vr.mHeadFollow.mDeadzoneDistanceM;
  const bool keepFollowing = state.mEngaged && (
    angErrDeg > vr.mHeadFollow.mDeadzoneAngleDeg * 0.2f
    || hmdMoveM > vr.mHeadFollow.mDeadzoneDistanceM * 0.2f);

  state.mEngaged = outside || keepFollowing;
  if (state.mEngaged) {
    state.mCurrentPose.mPosition = Vector3::Lerp(
      state.mCurrentPose.mPosition, targetPose.mPosition, alpha);
    state.mCurrentPose.mOrientation = Quaternion::Slerp(
      state.mCurrentPose.mOrientation, targetPose.mOrientation, alpha);
    state.mAnchorHmdPosition = Vector3::Lerp(
      state.mAnchorHmdPosition, hmdPose.mPosition, alpha);
  }
  return state.mCurrentPose;
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
  const VRRenderSettings&,
  const SHM::LayerConfig& layer) const {
  const auto& physicalSize = layer.mVR.mPhysicalSize;
  const auto virtualWidth = physicalSize.mWidth;
  const auto virtualHeight = physicalSize.mHeight;

  return {
    .mNormalSize = {virtualWidth, virtualHeight},
    .mZoomedSize =
      {virtualWidth * layer.mVR.mZoomScale,
       virtualHeight * layer.mVR.mZoomScale},
  };
}

void VRKneeboard::MaybeRecenter(
  const VRRenderSettings& vr,
  const Pose& hmdPose) {
  if (vr.mRecenterCount == mRecenterCount) {
    return;
  }
  this->Recenter(vr, hmdPose);
}

void VRKneeboard::Recenter(const VRRenderSettings& vr, const Pose& hmdPose) {
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
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const float dtSeconds) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }

  std::vector<Layer> ret;
  ret.reserve(layers.size());
  for (const auto& layerConfig: layers) {
    if (!layerConfig.mVREnabled) {
      continue;
    }

    ret.push_back(
      Layer {&layerConfig, GetRenderParameters(config, layerConfig, hmdPose, dtSeconds)});
  }

  if (!config.mVR.mEnableGazeInputFocus) {
    return ret;
  }
  const auto activeLayerID = config.mGlobalInputLayerID;

  const auto activeIt =
    std::ranges::find(ret, activeLayerID, [](const Layer& layer) {
      return layer.mLayerConfig->mLayerID;
    });
  if (
    (activeIt != ret.end())
    && (activeIt->mRenderParameters.mIsLookingAtKneeboard)) {
    return ret;
  }

  for (const auto& [layerConfig, renderParams]:
       std::ranges::reverse_view(ret)) {
    if (
      renderParams.mIsLookingAtKneeboard
      && layerConfig->mLayerID != activeLayerID) {
      SHM::ActiveConsumers::SetActiveInGameViewID(layerConfig->mLayerID);
      return ret;
    }
  }

  return ret;
}

VRKneeboard::RenderParameters VRKneeboard::GetRenderParameters(
  const SHM::Config& config,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose,
  const float dtSeconds) {
  const auto kneeboardPose = this->GetKneeboardPose(config.mVR, layer, hmdPose, dtSeconds);
  const auto isLookingAtKneeboard =
    this->IsLookingAtKneeboard(config, layer, hmdPose, kneeboardPose);

  return {
    .mKneeboardPose = kneeboardPose,
    .mKneeboardSize =
      this->GetKneeboardSize(config, layer, isLookingAtKneeboard),
    .mKneeboardOpacity = isLookingAtKneeboard ? layer.mVR.mOpacity.mGaze
                                              : layer.mVR.mOpacity.mNormal,
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
  auto currentSize =
    isLookingAtKneeboard ? sizes.mZoomedSize : sizes.mNormalSize;

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
