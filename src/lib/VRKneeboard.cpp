// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/RayIntersectsRect.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/VRKneeboard.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
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

Matrix MakeHandOffsetAnchor(
  const VRKneeboard::Pose& handMountPose,
  const VRHeadFollowSettings&) {
  return Matrix::CreateFromQuaternion(handMountPose.mOrientation)
    * Matrix::CreateTranslation(handMountPose.mPosition);
}

Quaternion QuatFromTo(const Vector3& fromDir, const Vector3& toDir) {
  Vector3 a = fromDir;
  Vector3 b = toDir;
  if (a.LengthSquared() < 1e-10f || b.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  a.Normalize();
  b.Normalize();
  const float dot = std::clamp(a.Dot(b), -1.0f, 1.0f);
  if (dot > 0.99999f) {
    return Quaternion::Identity;
  }
  if (dot < -0.99999f) {
    Vector3 ortho = Vector3::UnitX.Cross(a);
    if (ortho.LengthSquared() < 1e-8f) {
      ortho = Vector3::UnitY.Cross(a);
    }
    ortho.Normalize();
    return Quaternion(ortho.x, ortho.y, ortho.z, 0.0f);
  }
  const Vector3 c = a.Cross(b);
  Quaternion q(c.x, c.y, c.z, 1.0f + dot);
  q.Normalize();
  return q;
}

Vector3 ComputeWristOrbitDragPointDir(
  const Vector3& rayOrigin,
  const Vector3& rayDir,
  const float grabDistance,
  const Vector3& wristPos,
  const Vector3& fallbackDir) {
  Vector3 d = rayDir;
  if (d.LengthSquared() < 1e-10f) {
    return fallbackDir;
  }
  d.Normalize();
  const Vector3 dragPoint =
    rayOrigin + (d * (std::max)(0.05f, grabDistance));
  Vector3 rel = dragPoint - wristPos;
  if (rel.LengthSquared() > 1e-8f) {
    rel.Normalize();
    return rel;
  }
  return fallbackDir;
}

Quaternion ComputeWristLocalOrientation(
  const Vector3& localPos,
  const Vector3& forwardHint,
  const float twistDeg) {
  Vector3 up = localPos;
  if (up.LengthSquared() >= 1e-8f) {
    up.Normalize();
  } else {
    up = Vector3::UnitY;
  }

  const auto projectPerp = [&](const Vector3& v) {
    return v - (up * v.Dot(up));
  };

  Vector3 forwardProj = projectPerp(forwardHint);
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::Backward);
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::UnitX);
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::UnitY);
  }
  if (forwardProj.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  forwardProj.Normalize();

  if (std::abs(twistDeg) > 1e-4f) {
    const float rad = DirectX::XMConvertToRadians(twistDeg);
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const Vector3 uxF = up.Cross(forwardProj);
    forwardProj = (forwardProj * c) + (uxF * s);
    if (forwardProj.LengthSquared() > 1e-10f) {
      forwardProj.Normalize();
    }
  }

  Vector3 right = up.Cross(forwardProj);
  if (right.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  right.Normalize();

  Vector3 forwardZ = right.Cross(up);
  forwardZ.Normalize();
  const Matrix rotMat(
    right.x, right.y, right.z, 0.0f,
    up.x, up.y, up.z, 0.0f,
    forwardZ.x, forwardZ.y, forwardZ.z, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
  return Quaternion::CreateFromRotationMatrix(rotMat);
}

Quaternion ComputeWristAxisOrientation(
  const Vector3& boardPos,
  const Vector3& wristPos,
  const Vector3& eyePos,
  const Quaternion& handOrient,
  const Vector3* fallbackForwardWorld = nullptr) {
  Vector3 up = boardPos - wristPos;
  if (up.LengthSquared() >= 1e-8f) {
    up.Normalize();
  } else {
    up = Vector3::Transform(Vector3::UnitY, handOrient);
    if (up.LengthSquared() >= 1e-10f) {
      up.Normalize();
    } else {
      up = Vector3::UnitY;
    }
  }

  const auto projectPerp = [&](const Vector3& v) {
    return v - (up * v.Dot(up));
  };

  Vector3 forwardProj = projectPerp(eyePos - boardPos);
  if (forwardProj.LengthSquared() < (0.05f * 0.05f)) {
    if (fallbackForwardWorld) {
      const Vector3 fb = projectPerp(*fallbackForwardWorld);
      if (fb.LengthSquared() >= 1e-8f) {
        forwardProj = fb;
      }
    }
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::Transform(Vector3::Backward, handOrient));
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::Backward);
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::UnitX);
  }
  if (forwardProj.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  forwardProj.Normalize();

  Vector3 right = up.Cross(forwardProj);
  if (right.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  right.Normalize();

  Vector3 forwardZ = right.Cross(up);
  forwardZ.Normalize();
  const Matrix rotMat(
    right.x, right.y, right.z, 0.0f,
    up.x, up.y, up.z, 0.0f,
    forwardZ.x, forwardZ.y, forwardZ.z, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
  return Quaternion::CreateFromRotationMatrix(rotMat);
}

float QuaternionAngleDeg(const Quaternion& a, const Quaternion& b) {
  const float dot = std::clamp(std::abs(a.Dot(b)), 0.0f, 1.0f);
  return DirectX::XMConvertToDegrees(2.0f * std::acos(dot));
}

bool RayPlaneIntersection(
  const VRKneeboard::Pose& controllerPose,
  const Vector3& planePoint,
  const Vector3& planeNormal,
  float& outDistance,
  Vector3& outPoint) {
  auto rayDir = Vector3::Transform(Vector3::Forward, controllerPose.mOrientation);
  if (rayDir.LengthSquared() < 1e-6f) {
    return false;
  }
  rayDir.Normalize();
  const Ray ray(controllerPose.mPosition, rayDir);
  const Plane plane(planePoint, planeNormal);
  float dist = 0.0f;
  if (!ray.Intersects(plane, dist) || dist < 0.02f || dist > 6.0f) {
    return false;
  }
  outDistance = dist;
  outPoint = controllerPose.mPosition + (rayDir * dist);
  return true;
}

std::pair<float, float> GetCornerSigns(VRKneeboard::EditCorner corner) {
  switch (corner) {
    case VRKneeboard::EditCorner::NW:
      return {-1.0f, 1.0f};
    case VRKneeboard::EditCorner::NE:
      return {1.0f, 1.0f};
    case VRKneeboard::EditCorner::SW:
      return {-1.0f, -1.0f};
    case VRKneeboard::EditCorner::SE:
      return {1.0f, -1.0f};
    default:
      return {0.0f, 0.0f};
  }
}

}// namespace

VRRenderSettings VRKneeboard::GetEffectiveVRSettings(
  const VRRenderSettings& shmVR) const noexcept {
  auto ret = shmVR;
  if (!mManipulation.mHasOptimisticVR) {
    return ret;
  }
  const auto now = std::chrono::steady_clock::now();
  const bool dragging = mManipulation.mDragMode != ManipulationState::DragMode::None;
  const bool recent =
    (now - mManipulation.mLastChangeTime) < std::chrono::milliseconds(450);
  if (!dragging && !recent) {
    return ret;
  }
  if (
    !dragging
    && !(shmVR.mHeadFollow == mManipulation.mLastSeenSHMHeadFollow)) {
    return ret;
  }
  ret.mHeadFollow = mManipulation.mOptimisticHeadFollow;
  return ret;
}

void VRKneeboard::SolvePoseIntoSettings(
  VRHeadFollowSettings& hf,
  VRPose& worldPose,
  const Pose& targetPose,
  const Pose& hmdPose,
  const std::optional<Pose>& handMountPose) const {
  const auto effectiveMode = GetEffectiveFollowMode(hf);
  const auto targetMatrix =
    Matrix::CreateFromQuaternion(targetPose.mOrientation)
    * Matrix::CreateTranslation(targetPose.mPosition);

  if (effectiveMode == VRFollowMode::WorldLocked) {
    Matrix invRecenter = mRecenter.Invert();
    const auto localMatrix = targetMatrix * invRecenter;
    const float eyeHeight = mEyeHeight.value_or(hmdPose.mPosition.y);
    worldPose.mX = std::clamp(localMatrix._41, -1.6f, 1.6f);
    worldPose.mEyeY = std::clamp(localMatrix._42 - eyeHeight, -1.2f, 0.9f);
    worldPose.mZ = std::clamp(localMatrix._43, -1.6f, 0.8f);
    worldPose.mRY = std::asin(std::clamp(-localMatrix._13, -1.0f, 1.0f));
    if (std::abs(std::cos(worldPose.mRY)) < 1e-4f) {
      worldPose.mRX = 0.0f;
      worldPose.mRZ = std::atan2(-localMatrix._21, localMatrix._22);
    } else {
      worldPose.mRX = std::atan2(localMatrix._23, localMatrix._33);
      worldPose.mRZ = std::atan2(localMatrix._12, localMatrix._11);
    }
    return;
  }

  if (
    (effectiveMode == VRFollowMode::LeftHand
     || effectiveMode == VRFollowMode::RightHand)
    && handMountPose) {
    const auto anchor = MakeHandOffsetAnchor(*handMountPose, hf);
    const auto handLocal = targetMatrix * anchor.Invert();
    hf.mHandOffsetX = std::clamp(handLocal._41, -0.65f, 0.65f);
    hf.mHandOffsetY = std::clamp(handLocal._42, -0.45f, 0.65f);
    hf.mHandOffsetZ = std::clamp(handLocal._43, -0.65f, 0.65f);
    Vector3 localUp(hf.mHandOffsetX, hf.mHandOffsetY, hf.mHandOffsetZ);
    if (localUp.LengthSquared() >= 1e-8f) {
      localUp.Normalize();
    } else {
      localUp = Vector3::UnitY;
    }
    const auto unTilt = (std::abs(hf.mWristTiltOffset) > 1e-4f)
      ? Matrix::CreateRotationX(DirectX::XMConvertToRadians(-hf.mWristTiltOffset)) * handLocal
      : handLocal;
    const Vector3 localForwardRaw(unTilt._31, unTilt._32, unTilt._33);
    Vector3 perp = localForwardRaw - (localUp * localForwardRaw.Dot(localUp));
    if (perp.LengthSquared() >= 1e-8f) {
      perp.Normalize();
    } else {
      perp = Vector3::Backward;
    }
    hf.mHandForwardX = perp.x;
    hf.mHandForwardY = perp.y;
    hf.mHandForwardZ = perp.z;
    hf.mHandPitchDeg = 0.0f;
    return;
  }

  const auto anchor = MakeHorizonAlignedAnchor(hmdPose, hf);
  const auto headLocal = targetMatrix * anchor.Invert();
  hf.mHeadOffsetX = std::clamp(headLocal._41, -0.9f, 0.9f);
  hf.mHeadOffsetY = std::clamp(headLocal._42, -0.65f, 0.65f);
  hf.mHeadOffsetZ = std::clamp(headLocal._43, -1.35f, -0.20f);
  const float horiz = std::hypot(headLocal._31, headLocal._33);
  hf.mHeadPitchDeg = std::clamp(
    DirectX::XMConvertToDegrees(std::atan2(-headLocal._32, (std::max)(1e-5f, horiz))),
    -65.0f,
    65.0f);
  hf.mHeadYawDeg = std::clamp(
    DirectX::XMConvertToDegrees(std::atan2(headLocal._31, headLocal._33)),
    -80.0f,
    80.0f);
}

void VRKneeboard::SendManipulatedTransformEvent(const bool force) {
  const auto now = std::chrono::steady_clock::now();
  if (
    !force
    && (now - mManipulation.mLastPersistTime) < std::chrono::milliseconds(130)) {
    mManipulation.mDirtyPendingPersist = true;
    return;
  }

  mManipulation.mLastPersistTime = now;
  mManipulation.mDirtyPendingPersist = false;

  SetVRManipulatedTransformEvent ev {};
  ev.mKneeboard = mManipulation.mTargetKneeboardIndex;
  if (mManipulation.mHasOptimisticPose) {
    ev.mX = mManipulation.mOptimisticWorldPose.mX;
    ev.mEyeY = mManipulation.mOptimisticWorldPose.mEyeY;
    ev.mZ = mManipulation.mOptimisticWorldPose.mZ;
    ev.mRX = mManipulation.mOptimisticWorldPose.mRX;
    ev.mRY = mManipulation.mOptimisticWorldPose.mRY;
    ev.mRZ = mManipulation.mOptimisticWorldPose.mRZ;
  }
  if (mManipulation.mHasOptimisticVR) {
    const auto& hf = mManipulation.mOptimisticHeadFollow;
    ev.mHeadOffsetX = hf.mHeadOffsetX;
    ev.mHeadOffsetY = hf.mHeadOffsetY;
    ev.mHeadOffsetZ = hf.mHeadOffsetZ;
    ev.mHeadPitchDeg = hf.mHeadPitchDeg;
    ev.mHeadYawDeg = hf.mHeadYawDeg;
    ev.mHandOffsetX = hf.mHandOffsetX;
    ev.mHandOffsetY = hf.mHandOffsetY;
    ev.mHandOffsetZ = hf.mHandOffsetZ;
    ev.mHandPitchDeg = hf.mHandPitchDeg;
    ev.mHandForwardX = hf.mHandForwardX;
    ev.mHandForwardY = hf.mHandForwardY;
    ev.mHandForwardZ = hf.mHandForwardZ;
  }
  if (mManipulation.mHasOptimisticSize) {
    ev.mMaxWidth = mManipulation.mOptimisticSize.x;
    ev.mMaxHeight = mManipulation.mOptimisticSize.y;
  }
  APIEvent::FromStruct(ev).Send();
}

void VRKneeboard::UpdateControllerManipulation(
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const std::optional<Pose>& handMountPose,
  const ControllerInput& controller,
  const float dtSeconds) {
  const std::array<ControllerInput, 1> single {controller};
  this->UpdateControllerManipulation(
    config, layers, hmdPose, handMountPose, single, dtSeconds);
}

void VRKneeboard::UpdateControllerManipulation(
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const std::optional<Pose>& handMountPose,
  const std::span<const ControllerInput> controllers,
  const float dtSeconds) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
  mManipulation.mVisuals = {};

  // Pick active VR layer first so controller selection can check ray hits
  const SHM::LayerConfig* targetLayer = nullptr;
  uint8_t targetIndex = 0;
  uint8_t visibleIdx = 0;
  for (const auto& layer : layers) {
    if (!layer.mVREnabled) {
      continue;
    }
    ++visibleIdx;
    if (!targetLayer || layer.mLayerID == config.mGlobalInputLayerID) {
      targetLayer = &layer;
      targetIndex = visibleIdx;
    }
  }

  const auto effectiveVR = this->GetEffectiveVRSettings(config.mVR);
  bool anyGripHeld = false;
  for (const auto& c : controllers) {
    if (c.mValid && c.mGripHeld) {
      anyGripHeld = true;
      break;
    }
  }

  const bool editActive = effectiveVR.mAllowControllerManipulation
    && (effectiveVR.mEditModeEnabled || anyGripHeld);
  mManipulation.mEditModeActive = editActive;

  const auto sendContentLeave = [&]() {
    if (!mManipulation.mHasLastContentPointer) {
      return;
    }
    mManipulation.mHasLastContentPointer = false;
    mManipulation.mWasContentTriggerDown = false;
    VRContentPointerEvent ev {};
    ev.mKneeboard = targetIndex;
    ev.mLayerID = mManipulation.mTargetLayerID;
    ev.mAction = VRContentPointerEvent::Action::Leave;
    APIEvent::FromStruct(ev).Send();
  };

  if (!targetLayer) {
    mManipulation.mHoveredCorner = EditCorner::None;
    mManipulation.mHoveredSurface = false;
    if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
      mManipulation.mDragMode = ManipulationState::DragMode::None;
      this->SendManipulatedTransformEvent(true);
    }
    mManipulation.mWasTriggerDown = false;
    sendContentLeave();
    return;
  }

  if (!editActive) {
    mManipulation.mHoveredCorner = EditCorner::None;
    mManipulation.mHoveredSurface = false;
    if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
      mManipulation.mDragMode = ManipulationState::DragMode::None;
      this->SendManipulatedTransformEvent(true);
    }
    mManipulation.mWasTriggerDown = false;

    SHM::Config effectiveCfg = config;
    effectiveCfg.mVR = effectiveVR;
    const Pose contentPose = this->GetKneeboardPose(
      effectiveVR, *targetLayer, hmdPose, 0.0f, handMountPose);
    const bool contentLooking = this->IsLookingAtKneeboard(
      effectiveCfg, *targetLayer, hmdPose, contentPose);
    const Vector2 contentSize =
      this->GetKneeboardSize(effectiveCfg, *targetLayer, contentLooking);
    const auto cRight =
      Vector3::Transform(Vector3::UnitX, contentPose.mOrientation);
    const auto cUp =
      Vector3::Transform(Vector3::UnitY, contentPose.mOrientation);
    const auto cNormal =
      Vector3::Transform(Vector3::Backward, contentPose.mOrientation);

    ControllerInput pointerCtrl {};
    float bestHitDist = 0.0f;
    Vector3 bestHitPt {};
    float bestU = 0.5f;
    float bestV = 0.5f;
    int bestScore = -1;
    for (const auto& c : controllers) {
      if (!c.mValid) {
        continue;
      }
      float d = 0.0f;
      Vector3 pt {};
      if (!RayPlaneIntersection(
            c.mAimPose, contentPose.mPosition, cNormal, d, pt)) {
        continue;
      }
      const Vector3 rel = pt - contentPose.mPosition;
      const float lx = rel.Dot(cRight);
      const float ly = rel.Dot(cUp);
      if (
        std::abs(lx) > contentSize.x * 0.5f
        || std::abs(ly) > contentSize.y * 0.5f) {
        continue;
      }
      int score = 10 + (c.mTriggerHeld ? 100 : 0)
        + (c.mHandIndex == mManipulation.mActiveHandIndex ? 5 : 0);
      if (score > bestScore) {
        bestScore = score;
        pointerCtrl = c;
        bestHitDist = d;
        bestHitPt = pt;
        bestU = std::clamp(
          (lx / (std::max)(0.05f, contentSize.x)) + 0.5f, 0.0f, 1.0f);
        bestV = std::clamp(
          0.5f - (ly / (std::max)(0.05f, contentSize.y)), 0.0f, 1.0f);
      }
    }

    if (!pointerCtrl.mValid) {
      sendContentLeave();
      return;
    }

    auto rayDir = Vector3::Transform(
      Vector3::Forward, pointerCtrl.mAimPose.mOrientation);
    if (rayDir.LengthSquared() > 1e-6f) {
      rayDir.Normalize();
      auto& vis = mManipulation.mVisuals;
      vis.mEditModeActive = false;
      vis.mContentPointerActive = true;
      vis.mHasRay = true;
      vis.mHasHitOnBoard = true;
      vis.mTargetLayerID = targetLayer->mLayerID;
      vis.mHitLocalU = bestU;
      vis.mHitLocalV = bestV;
      vis.mRayStartWorld = pointerCtrl.mAimPose.mPosition;
      vis.mRayEndWorld =
        (bestHitDist > 0.02f)
          ? (bestHitPt - (rayDir * 0.004f))
          : bestHitPt;
    }

    mManipulation.mActiveHandIndex = pointerCtrl.mHandIndex;
    mManipulation.mTargetLayerID = targetLayer->mLayerID;
    const auto nowPtr = std::chrono::steady_clock::now();
    const bool triggerDown = pointerCtrl.mTriggerHeld;
    const bool wasDown = mManipulation.mWasContentTriggerDown;
    const uint32_t buttons = triggerDown ? 1u : 0u;
    const bool uvChanged = !mManipulation.mHasLastContentPointer
      || std::abs(bestU - mManipulation.mLastContentU) > 0.0015f
      || std::abs(bestV - mManipulation.mLastContentV) > 0.0015f;
    const bool moveThrottle =
      (nowPtr - mManipulation.mLastContentSendTime)
      >= std::chrono::milliseconds(18);

    if (triggerDown != wasDown) {
      VRContentPointerEvent ev {};
      ev.mKneeboard = targetIndex;
      ev.mLayerID = targetLayer->mLayerID;
      ev.mAction = triggerDown
        ? VRContentPointerEvent::Action::PrimaryDown
        : VRContentPointerEvent::Action::PrimaryUp;
      ev.mU = bestU;
      ev.mV = bestV;
      ev.mButtons = buttons;
      APIEvent::FromStruct(ev).Send();
      mManipulation.mLastContentSendTime = nowPtr;
      mManipulation.mLastContentU = bestU;
      mManipulation.mLastContentV = bestV;
      mManipulation.mHasLastContentPointer = true;
    } else if (uvChanged && moveThrottle) {
      VRContentPointerEvent ev {};
      ev.mKneeboard = targetIndex;
      ev.mLayerID = targetLayer->mLayerID;
      ev.mAction = VRContentPointerEvent::Action::Move;
      ev.mU = bestU;
      ev.mV = bestV;
      ev.mButtons = buttons;
      APIEvent::FromStruct(ev).Send();
      mManipulation.mLastContentSendTime = nowPtr;
      mManipulation.mLastContentU = bestU;
      mManipulation.mLastContentV = bestV;
      mManipulation.mHasLastContentPointer = true;
    }
    mManipulation.mWasContentTriggerDown = triggerDown;

    if (
      std::abs(pointerCtrl.mThumbstickY) > 0.25f
      && (nowPtr - mManipulation.mLastContentWheelTime)
        >= std::chrono::milliseconds(60)) {
      VRContentPointerEvent ev {};
      ev.mKneeboard = targetIndex;
      ev.mLayerID = targetLayer->mLayerID;
      ev.mAction = VRContentPointerEvent::Action::Wheel;
      ev.mU = bestU;
      ev.mV = bestV;
      ev.mButtons = buttons;
      ev.mWheelDeltaY = static_cast<int32_t>(
        std::lround(pointerCtrl.mThumbstickY * 120.0f));
      APIEvent::FromStruct(ev).Send();
      mManipulation.mLastContentWheelTime = nowPtr;
      mManipulation.mHasLastContentPointer = true;
    }
    return;
  }
  sendContentLeave();

  const auto now = std::chrono::steady_clock::now();
  const bool recentOptimistic =
    (now - mManipulation.mLastChangeTime) < std::chrono::milliseconds(450);
  if (
    mManipulation.mDragMode == ManipulationState::DragMode::None
    && !recentOptimistic) {
    mManipulation.mHasOptimisticVR = false;
    mManipulation.mHasOptimisticPose = false;
    mManipulation.mHasOptimisticSize = false;
  }
  const bool useOptimistic =
    mManipulation.mDragMode != ManipulationState::DragMode::None
    || recentOptimistic;
  SHM::LayerConfig workingLayer = *targetLayer;
  if (
    useOptimistic && mManipulation.mHasOptimisticPose
    && workingLayer.mLayerID == mManipulation.mTargetLayerID) {
    workingLayer.mVR.mPose = mManipulation.mOptimisticWorldPose;
  }
  Pose boardPose = this->GetKneeboardPose(
    effectiveVR, workingLayer, hmdPose, 0.0f, handMountPose);
  SHM::Config effectiveCfg = config;
  effectiveCfg.mVR = effectiveVR;
  if (
    useOptimistic && mManipulation.mHasOptimisticSize
    && workingLayer.mLayerID == mManipulation.mTargetLayerID
    && mManipulation.mOptimisticSize.x > 0.05f
    && mManipulation.mOptimisticSize.y > 0.05f) {
    workingLayer.mVR.mPhysicalSize = {
      mManipulation.mOptimisticSize.x,
      mManipulation.mOptimisticSize.y,
    };
  }
  const bool isLooking = this->IsLookingAtKneeboard(
    effectiveCfg, workingLayer, hmdPose, boardPose);
  const bool isZoomed = effectiveVR.mForceZoom
    || (isLooking && workingLayer.mVR.mEnableGazeZoom);
  const float zoomFactor = isZoomed
    ? (std::max)(0.25f, workingLayer.mVR.mZoomScale)
    : 1.0f;
  Vector2 boardSize =
    this->GetKneeboardSize(effectiveCfg, workingLayer, isLooking);

  const auto boardRight =
    Vector3::Transform(Vector3::UnitX, boardPose.mOrientation);
  const auto boardUp = Vector3::Transform(Vector3::UnitY, boardPose.mOrientation);
  const auto boardNormal =
    Vector3::Transform(Vector3::Backward, boardPose.mOrientation);

  // Select best controller among candidates:
  // 1) Locked hand while dragging
  // 2) Controller holding Trigger or Grip
  // 3) Controller whose ray hits the board
  // 4) First valid controller
  ControllerInput controller {};
  if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
    for (const auto& c : controllers) {
      if (c.mValid && c.mHandIndex == mManipulation.mActiveHandIndex) {
        controller = c;
        break;
      }
    }
  }
  if (!controller.mValid) {
    int bestScore = -1;
    for (const auto& c : controllers) {
      if (!c.mValid) {
        continue;
      }
      int score = 0;
      if (c.mTriggerHeld) {
        score += 100;
      }
      if (c.mGripHeld) {
        score += 50;
      }
      float d = 0.0f;
      Vector3 pt {};
      if (RayPlaneIntersection(c.mAimPose, boardPose.mPosition, boardNormal, d, pt)) {
        const Vector3 rel = pt - boardPose.mPosition;
        const float lx = rel.Dot(boardRight);
        const float ly = rel.Dot(boardUp);
        if (
          std::abs(lx) <= (boardSize.x * 0.5f) + 0.015f
          && std::abs(ly) <= (boardSize.y * 0.5f) + 0.015f) {
          score += 20;
        }
      }
      if (std::abs(c.mThumbstickY) > 0.25f) {
        score += 5;
      }
      if (score > bestScore) {
        bestScore = score;
        controller = c;
      }
    }
  }

  mManipulation.mHoveredCorner = EditCorner::None;
  mManipulation.mHoveredSurface = false;

  if (
    mManipulation.mDragMode == ManipulationState::DragMode::None
    && controller.mValid) {
    float hitDist = 0.0f;
    Vector3 hitPoint {};
    if (RayPlaneIntersection(
          controller.mAimPose, boardPose.mPosition, boardNormal, hitDist, hitPoint)) {
      const Vector3 rel = hitPoint - boardPose.mPosition;
      const float localX = rel.Dot(boardRight);
      const float localY = rel.Dot(boardUp);
      const float halfW = boardSize.x * 0.5f;
      const float halfH = boardSize.y * 0.5f;
      const float cornerRadius =
        (std::max)(0.026f, (std::min)(boardSize.x, boardSize.y) * 0.12f);

      struct Candidate {
        EditCorner mCorner;
        float mX;
        float mY;
      };
      const std::array<Candidate, 4> corners {{
        {EditCorner::NW, -halfW, halfH},
        {EditCorner::NE, halfW, halfH},
        {EditCorner::SW, -halfW, -halfH},
        {EditCorner::SE, halfW, -halfH},
      }};
      float bestDist = cornerRadius;
      for (const auto& c : corners) {
        const float d = std::hypot(localX - c.mX, localY - c.mY);
        if (d <= bestDist) {
          bestDist = d;
          mManipulation.mHoveredCorner = c.mCorner;
        }
      }
      if (
        mManipulation.mHoveredCorner != EditCorner::None
        || (std::abs(localX) <= halfW + 0.015f
            && std::abs(localY) <= halfH + 0.015f)) {
        mManipulation.mHoveredSurface = true;
      }

      const bool justPressed =
        controller.mTriggerHeld && !mManipulation.mWasTriggerDown;
      if (justPressed && mManipulation.mHoveredSurface) {
        mManipulation.mActiveHandIndex = controller.mHandIndex;
        mManipulation.mTargetLayerID = workingLayer.mLayerID;
        mManipulation.mTargetKneeboardIndex = targetIndex;
        mManipulation.mGrabDistanceM = hitDist;
        mManipulation.mGrabLocalU =
          std::clamp((localX / (std::max)(0.05f, boardSize.x)) + 0.5f, 0.0f, 1.0f);
        mManipulation.mGrabLocalV =
          std::clamp(0.5f - (localY / (std::max)(0.05f, boardSize.y)), 0.0f, 1.0f);
        mManipulation.mGrabBoardStartOrient = boardPose.mOrientation;
        mManipulation.mGrabStartSize = boardSize;
        mManipulation.mGrabZoomFactor = zoomFactor;
        if (!mManipulation.mHasOptimisticVR) {
          mManipulation.mOptimisticHeadFollow = effectiveVR.mHeadFollow;
        }
        mManipulation.mLastSeenSHMHeadFollow = config.mVR.mHeadFollow;
        if (!mManipulation.mHasOptimisticPose) {
          mManipulation.mOptimisticWorldPose = workingLayer.mVR.mPose;
        }
        if (!mManipulation.mHasOptimisticSize) {
          mManipulation.mOptimisticSize = {
            boardSize.x / zoomFactor,
            boardSize.y / zoomFactor,
          };
        }

        if (mManipulation.mHoveredCorner != EditCorner::None) {
          switch (mManipulation.mHoveredCorner) {
            case EditCorner::NW:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeNW;
              break;
            case EditCorner::NE:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeNE;
              break;
            case EditCorner::SW:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeSW;
              break;
            case EditCorner::SE:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeSE;
              break;
            default:
              break;
          }
          const auto [u, v] = GetCornerSigns(mManipulation.mHoveredCorner);
          mManipulation.mGrabAnchorCornerWorld = boardPose.mPosition
            - (boardRight * (u * halfW))
            - (boardUp * (v * halfH));
        } else {
          mManipulation.mDragMode = ManipulationState::DragMode::Move;
          mManipulation.mGrabPositionOffset = boardPose.mPosition - hitPoint;
          Quaternion invCtrl = controller.mAimPose.mOrientation;
          invCtrl.Inverse(invCtrl);
          mManipulation.mGrabControllerInv = invCtrl;
          const auto effectiveStartMode =
            GetEffectiveFollowMode(mManipulation.mOptimisticHeadFollow);
          if (
            (effectiveStartMode == VRFollowMode::LeftHand
             || effectiveStartMode == VRFollowMode::RightHand)
            && handMountPose) {
            const Vector3 radial0 = boardPose.mPosition - handMountPose->mPosition;
            const float radius0 = std::clamp(radial0.Length(), 0.06f, 0.65f);
            mManipulation.mGrabHandRadiusM = radius0;
            Vector3 boardDir0 =
              (radial0.LengthSquared() > 1e-8f) ? radial0 : Vector3::UnitY;
            boardDir0.Normalize();
            Vector3 rayDir0 = Vector3::Transform(
              Vector3::Forward, controller.mAimPose.mOrientation);
            if (rayDir0.LengthSquared() > 1e-8f) {
              rayDir0.Normalize();
            } else {
              rayDir0 = Vector3::Forward;
            }
            const Vector3 grabDir0 = ComputeWristOrbitDragPointDir(
              controller.mAimPose.mPosition,
              rayDir0,
              hitDist,
              handMountPose->mPosition,
              boardDir0);
            mManipulation.mGrabHandOrbitOffset =
              QuatFromTo(grabDir0, boardDir0);
          } else {
            mManipulation.mGrabHandOrbitOffset = Quaternion::Identity;
          }
        }
      }
    }
  } else if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
    if (!controller.mValid || !controller.mTriggerHeld) {
      mManipulation.mDragMode = ManipulationState::DragMode::None;
      this->SendManipulatedTransformEvent(true);
    } else if (mManipulation.mDragMode == ManipulationState::DragMode::Move) {
      const auto effectiveDragMode =
        GetEffectiveFollowMode(mManipulation.mOptimisticHeadFollow);
      const bool isHandPivotDrag =
        (effectiveDragMode == VRFollowMode::LeftHand
         || effectiveDragMode == VRFollowMode::RightHand)
        && handMountPose;

      auto rayDir =
        Vector3::Transform(Vector3::Forward, controller.mAimPose.mOrientation);
      if (rayDir.LengthSquared() > 1e-8f) {
        rayDir.Normalize();
      } else {
        rayDir = Vector3::Forward;
      }

      Vector3 targetPos {};
      Quaternion targetOrient {};
      if (isHandPivotDrag) {
        if (std::abs(controller.mThumbstickY) > 0.18f) {
          mManipulation.mGrabHandRadiusM = std::clamp(
            mManipulation.mGrabHandRadiusM
              + (controller.mThumbstickY * 0.45f * dtSeconds),
            0.06f,
            0.65f);
        }
        const Vector3 rawDir = ComputeWristOrbitDragPointDir(
          controller.mAimPose.mPosition,
          rayDir,
          mManipulation.mGrabDistanceM,
          handMountPose->mPosition,
          Vector3::UnitY);
        Vector3 boardDir =
          Vector3::Transform(rawDir, mManipulation.mGrabHandOrbitOffset);
        if (boardDir.LengthSquared() > 1e-8f) {
          boardDir.Normalize();
        } else {
          boardDir = rawDir;
        }
        targetPos = handMountPose->mPosition
          + (boardDir * mManipulation.mGrabHandRadiusM);
        const Vector3 prevForward = boardNormal;
        const Quaternion axisOrient = ComputeWristAxisOrientation(
          targetPos,
          handMountPose->mPosition,
          hmdPose.mPosition,
          handMountPose->mOrientation,
          &prevForward);
        const float tiltDeg = mManipulation.mOptimisticHeadFollow.mWristTiltOffset;
        targetOrient = (std::abs(tiltDeg) > 1e-4f)
          ? (Quaternion::CreateFromAxisAngle(
               Vector3::UnitX, DirectX::XMConvertToRadians(tiltDeg))
             * axisOrient)
          : axisOrient;
        mManipulation.mGrabDistanceM = std::clamp(
          (targetPos - controller.mAimPose.mPosition).Length(), 0.06f, 2.5f);
      } else {
        if (std::abs(controller.mThumbstickY) > 0.18f) {
          mManipulation.mGrabDistanceM = std::clamp(
            mManipulation.mGrabDistanceM
              + (controller.mThumbstickY * 0.65f * dtSeconds),
            0.22f,
            1.45f);
        }
        const Vector3 targetHit =
          controller.mAimPose.mPosition + (rayDir * mManipulation.mGrabDistanceM);
        const Quaternion deltaRot =
          mManipulation.mGrabControllerInv * controller.mAimPose.mOrientation;
        targetOrient = mManipulation.mGrabBoardStartOrient * deltaRot;
        const Vector3 targetRight =
          Vector3::Transform(Vector3::UnitX, targetOrient);
        const Vector3 targetUp =
          Vector3::Transform(Vector3::UnitY, targetOrient);
        const float offsetX = (mManipulation.mGrabLocalU - 0.5f) * boardSize.x;
        const float offsetY = (0.5f - mManipulation.mGrabLocalV) * boardSize.y;
        targetPos = targetHit
          - (targetRight * offsetX)
          - (targetUp * offsetY);
      }
      const Pose targetPose {
        .mPosition = targetPos,
        .mOrientation = targetOrient,
      };
      mManipulation.mHasOptimisticVR = true;
      mManipulation.mHasOptimisticPose = true;
      this->SolvePoseIntoSettings(
        mManipulation.mOptimisticHeadFollow,
        mManipulation.mOptimisticWorldPose,
        targetPose,
        hmdPose,
        handMountPose);
      mManipulation.mLastChangeTime = std::chrono::steady_clock::now();
      this->SendManipulatedTransformEvent(false);
    } else {
      // Corner resize anchoring the opposite corner in world space
      EditCorner corner = EditCorner::SE;
      switch (mManipulation.mDragMode) {
        case ManipulationState::DragMode::ResizeNW:
          corner = EditCorner::NW;
          break;
        case ManipulationState::DragMode::ResizeNE:
          corner = EditCorner::NE;
          break;
        case ManipulationState::DragMode::ResizeSW:
          corner = EditCorner::SW;
          break;
        case ManipulationState::DragMode::ResizeSE:
          corner = EditCorner::SE;
          break;
        default:
          break;
      }
      const auto [u, v] = GetCornerSigns(corner);
      const auto anchorRight =
        Vector3::Transform(Vector3::UnitX, mManipulation.mGrabBoardStartOrient);
      const auto anchorUp =
        Vector3::Transform(Vector3::UnitY, mManipulation.mGrabBoardStartOrient);
      const auto anchorNormal = Vector3::Transform(
        Vector3::Backward, mManipulation.mGrabBoardStartOrient);

      float hitDist = 0.0f;
      Vector3 hitWorld {};
      if (RayPlaneIntersection(
            controller.mAimPose,
            mManipulation.mGrabAnchorCornerWorld,
            anchorNormal,
            hitDist,
            hitWorld)) {
        const Vector3 delta = hitWorld - mManipulation.mGrabAnchorCornerWorld;
        const float rawW = delta.Dot(anchorRight) * u;
        const float rawH = delta.Dot(anchorUp) * v;
        const float aspect = mManipulation.mGrabStartSize.x
          / (std::max)(0.05f, mManipulation.mGrabStartSize.y);
        const float grabZ = (std::max)(0.25f, mManipulation.mGrabZoomFactor);
        const float invZ = 1.0f / grabZ;
        float newH = std::clamp(
          (rawH + (rawW / aspect)) * 0.5f, 0.12f * grabZ, 0.85f * grabZ);
        float newW = std::clamp(newH * aspect, 0.12f * grabZ, 0.85f * grabZ);
        newH = newW / aspect;

        const float unzoomedW = std::clamp(newW * invZ, 0.10f, 0.85f);
        const float unzoomedH = std::clamp(newH * invZ, 0.10f, 0.85f);
        newW = unzoomedW * grabZ;
        newH = unzoomedH * grabZ;
        boardSize = {newW, newH};

        const Vector3 newCenter = mManipulation.mGrabAnchorCornerWorld
          + (anchorRight * (u * newW * 0.5f))
          + (anchorUp * (v * newH * 0.5f));
        const Pose targetPose {
          .mPosition = newCenter,
          .mOrientation = mManipulation.mGrabBoardStartOrient,
        };
        mManipulation.mHasOptimisticVR = true;
        mManipulation.mHasOptimisticPose = true;
        mManipulation.mHasOptimisticSize = true;
        mManipulation.mOptimisticSize = {unzoomedW, unzoomedH};
        this->SolvePoseIntoSettings(
          mManipulation.mOptimisticHeadFollow,
          mManipulation.mOptimisticWorldPose,
          targetPose,
          hmdPose,
          handMountPose);
        mManipulation.mLastChangeTime = std::chrono::steady_clock::now();
        this->SendManipulatedTransformEvent(false);
      }
    }
  }

  // Thumbstick depth nudge while hovering in Edit Mode without dragging
  if (
    mManipulation.mDragMode == ManipulationState::DragMode::None
    && controller.mValid && mManipulation.mHoveredSurface
    && std::abs(controller.mThumbstickY) > 0.25f) {
    if (!mManipulation.mHasOptimisticVR) {
      mManipulation.mOptimisticHeadFollow = effectiveVR.mHeadFollow;
    }
    mManipulation.mLastSeenSHMHeadFollow = config.mVR.mHeadFollow;
    if (!mManipulation.mHasOptimisticPose) {
      mManipulation.mOptimisticWorldPose = workingLayer.mVR.mPose;
    }
    mManipulation.mTargetLayerID = workingLayer.mLayerID;
    mManipulation.mTargetKneeboardIndex = targetIndex;

    auto& hf = mManipulation.mOptimisticHeadFollow;
    const auto effectiveMode = GetEffectiveFollowMode(hf);
    const float deltaM = controller.mThumbstickY * 0.55f * dtSeconds;
    if (effectiveMode == VRFollowMode::WorldLocked) {
      Vector3 radial = boardPose.mPosition - hmdPose.mPosition;
      const float len = radial.Length();
      if (len > 0.05f) {
        radial /= len;
        const float nextLen = std::clamp(len + deltaM, 0.22f, 1.45f);
        Pose targetPose = boardPose;
        targetPose.mPosition = hmdPose.mPosition + (radial * nextLen);
        this->SolvePoseIntoSettings(
          hf,
          mManipulation.mOptimisticWorldPose,
          targetPose,
          hmdPose,
          handMountPose);
      }
    } else if (
      effectiveMode == VRFollowMode::LeftHand
      || effectiveMode == VRFollowMode::RightHand) {
      const Vector3 v(hf.mHandOffsetX, hf.mHandOffsetY, hf.mHandOffsetZ);
      const float curR = v.Length();
      const Vector3 dir = (curR > 1e-4f) ? (v / curR) : Vector3::UnitY;
      const float nextR =
        std::clamp(((curR > 1e-4f) ? curR : 0.18f) + deltaM, 0.06f, 0.65f);
      hf.mHandOffsetX = std::clamp(dir.x * nextR, -0.65f, 0.65f);
      hf.mHandOffsetY = std::clamp(dir.y * nextR, -0.45f, 0.65f);
      hf.mHandOffsetZ = std::clamp(dir.z * nextR, -0.65f, 0.65f);
    } else {
      hf.mHeadOffsetZ = std::clamp(hf.mHeadOffsetZ - deltaM, -1.35f, -0.20f);
    }
    mManipulation.mHasOptimisticVR = true;
    mManipulation.mHasOptimisticPose = true;
    mManipulation.mLastChangeTime = std::chrono::steady_clock::now();
    this->SendManipulatedTransformEvent(false);
  }

  mManipulation.mWasTriggerDown = controller.mTriggerHeld;

  auto& vis = mManipulation.mVisuals;
  vis.mEditModeActive = true;
  vis.mDragging =
    mManipulation.mDragMode != ManipulationState::DragMode::None;
  vis.mHoveredCorner = mManipulation.mHoveredCorner;
  vis.mTargetLayerID = workingLayer.mLayerID;
  if (
    vis.mDragging
    && mManipulation.mDragMode != ManipulationState::DragMode::Move) {
    switch (mManipulation.mDragMode) {
      case ManipulationState::DragMode::ResizeNW:
        vis.mHoveredCorner = EditCorner::NW;
        break;
      case ManipulationState::DragMode::ResizeNE:
        vis.mHoveredCorner = EditCorner::NE;
        break;
      case ManipulationState::DragMode::ResizeSW:
        vis.mHoveredCorner = EditCorner::SW;
        break;
      case ManipulationState::DragMode::ResizeSE:
        vis.mHoveredCorner = EditCorner::SE;
        break;
      default:
        break;
    }
  }

  if (controller.mValid) {
    auto rayDir =
      Vector3::Transform(Vector3::Forward, controller.mAimPose.mOrientation);
    if (rayDir.LengthSquared() > 1e-6f) {
      rayDir.Normalize();
      vis.mRayStartWorld = controller.mAimPose.mPosition;

      SHM::LayerConfig visLayer = workingLayer;
      if (mManipulation.mHasOptimisticPose) {
        visLayer.mVR.mPose = mManipulation.mOptimisticWorldPose;
      }
      const auto visEffectiveVR = this->GetEffectiveVRSettings(config.mVR);
      const Pose visBoardPose = this->GetKneeboardPose(
        visEffectiveVR, visLayer, hmdPose, 0.0f, handMountPose);
      const Vector3 visRight =
        Vector3::Transform(Vector3::UnitX, visBoardPose.mOrientation);
      const Vector3 visUp =
        Vector3::Transform(Vector3::UnitY, visBoardPose.mOrientation);

      if (mManipulation.mDragMode == ManipulationState::DragMode::Move) {
        vis.mHasRay = true;
        vis.mHasHitOnBoard = true;
        vis.mHitLocalU = mManipulation.mGrabLocalU;
        vis.mHitLocalV = mManipulation.mGrabLocalV;
        const float visOffX = (vis.mHitLocalU - 0.5f) * boardSize.x;
        const float visOffY = (0.5f - vis.mHitLocalV) * boardSize.y;
        const Vector3 grabWorld = visBoardPose.mPosition
          + (visRight * visOffX) + (visUp * visOffY);
        const Vector3 beamDelta = grabWorld - vis.mRayStartWorld;
        const float beamDist = beamDelta.Length();
        vis.mRayEndWorld = (beamDist > 0.04f)
          ? (vis.mRayStartWorld + (beamDelta / beamDist) * (beamDist - 0.004f))
          : grabWorld;
      } else if (vis.mDragging) {
        vis.mHasRay = true;
        const auto [u, v] = GetCornerSigns(vis.mHoveredCorner);
        vis.mHasHitOnBoard = true;
        vis.mHitLocalU = (u > 0.0f) ? 1.0f : 0.0f;
        vis.mHitLocalV = (v > 0.0f) ? 0.0f : 1.0f;
        const Vector3 cornerWorld = visBoardPose.mPosition
          + (visRight * (u * boardSize.x * 0.5f))
          + (visUp * (v * boardSize.y * 0.5f));
        const Vector3 beamDelta = cornerWorld - vis.mRayStartWorld;
        const float beamDist = beamDelta.Length();
        vis.mRayEndWorld = (beamDist > 0.04f)
          ? (vis.mRayStartWorld + (beamDelta / beamDist) * (beamDist - 0.004f))
          : cornerWorld;
      } else {
        float hitDist = 0.0f;
        Vector3 hitPt {};
        if (RayPlaneIntersection(
              controller.mAimPose,
              boardPose.mPosition,
              boardNormal,
              hitDist,
              hitPt)) {
          const Vector3 rel = hitPt - boardPose.mPosition;
          const float localX = rel.Dot(boardRight);
          const float localY = rel.Dot(boardUp);
          const float halfW = boardSize.x * 0.5f;
          const float halfH = boardSize.y * 0.5f;
          if (
            (std::abs(localX) <= halfW + 0.015f
             && std::abs(localY) <= halfH + 0.015f)
            || mManipulation.mHoveredCorner != EditCorner::None) {
            vis.mHasRay = true;
            vis.mHasHitOnBoard = true;
            vis.mHitLocalU = std::clamp(
              (localX / (std::max)(0.05f, boardSize.x)) + 0.5f, 0.0f, 1.0f);
            vis.mHitLocalV = std::clamp(
              0.5f - (localY / (std::max)(0.05f, boardSize.y)), 0.0f, 1.0f);
            vis.mRayEndWorld = hitPt - (rayDir * 0.004f);
          }
        }
      }
    }
  }
}

VRKneeboard::Pose VRKneeboard::GetKneeboardPose(
  const VRRenderSettings& vr,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose,
  const float dtSeconds,
  const std::optional<Pose>& handMountPose) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
  const auto effectiveMode = GetEffectiveFollowMode(vr.mHeadFollow);
  const auto& pose = layer.mVR.mPose;
  this->MaybeRecenter(vr, hmdPose);
  if (effectiveMode == VRFollowMode::WorldLocked) {
    const auto matrix = Matrix::CreateRotationX(pose.mRX)
      * Matrix::CreateRotationY(pose.mRY) * Matrix::CreateRotationZ(pose.mRZ)
      * Matrix::CreateTranslation({
        pose.mX,
        pose.mEyeY + *mEyeHeight,
        pose.mZ,
      })
      * mRecenter;
    auto& state = mFollowStates[layer.mLayerID];
    state.mLastEffectiveMode = effectiveMode;
    return {
      .mPosition = matrix.Translation(),
      .mOrientation = Quaternion::CreateFromRotationMatrix(matrix),
    };
  }

  auto& state = mFollowStates[layer.mLayerID];
  const bool modeChanged =
    state.mInitialized && state.mLastEffectiveMode != effectiveMode;
  state.mLastEffectiveMode = effectiveMode;
  const float clampedDt = std::clamp(dtSeconds, 0.0f, 0.1f);
  const float alpha = (dtSeconds <= 0.0f)
    ? 0.0f
    : (vr.mHeadFollow.mFollowSpeed >= 20.0f
         ? 1.0f
         : 1.0f - std::exp(-vr.mHeadFollow.mFollowSpeed * clampedDt));

  if (
    effectiveMode == VRFollowMode::LeftHand
    || effectiveMode == VRFollowMode::RightHand) {
    const auto& hf = vr.mHeadFollow;
    state.mLastHandOffsetX = hf.mHandOffsetX;
    state.mLastHandOffsetY = hf.mHandOffsetY;
    state.mLastHandOffsetZ = hf.mHandOffsetZ;
    state.mLastHandPitchDeg = hf.mHandPitchDeg;
    state.mLastHandForwardX = hf.mHandForwardX;
    state.mLastHandForwardY = hf.mHandForwardY;
    state.mLastHandForwardZ = hf.mHandForwardZ;

    if (handMountPose) {
      const Vector3 localPos(hf.mHandOffsetX, hf.mHandOffsetY, hf.mHandOffsetZ);
      const Vector3 forwardHint(
        hf.mHandForwardX, hf.mHandForwardY, hf.mHandForwardZ);
      const Quaternion localOrient =
        ComputeWristLocalOrientation(localPos, forwardHint, hf.mHandPitchDeg);
      const auto tiltMatrix = (std::abs(hf.mWristTiltOffset) > 1e-4f)
        ? Matrix::CreateRotationX(DirectX::XMConvertToRadians(hf.mWristTiltOffset))
        : Matrix::Identity;
      const auto handLocalMatrix =
        tiltMatrix
        * Matrix::CreateFromQuaternion(localOrient)
        * Matrix::CreateTranslation(localPos);
      const auto anchor = MakeHandOffsetAnchor(*handMountPose, hf);
      const auto targetMatrix = handLocalMatrix * anchor;
      const Pose targetPose {
        .mPosition = targetMatrix.Translation(),
        .mOrientation = Quaternion::CreateFromRotationMatrix(targetMatrix),
      };
      state.mInitialized = true;
      state.mCurrentPose = targetPose;
      state.mAnchorHmdPosition = hmdPose.mPosition;
      return state.mCurrentPose;
    }
    if (state.mInitialized && !modeChanged) {
      return state.mCurrentPose;
    }
  }

  const auto& hf = vr.mHeadFollow;
  const auto headLocalMatrix =
    Matrix::CreateRotationX(DirectX::XMConvertToRadians(hf.mHeadPitchDeg))
    * Matrix::CreateRotationY(DirectX::XMConvertToRadians(hf.mHeadYawDeg))
    * Matrix::CreateTranslation({
      hf.mHeadOffsetX,
      hf.mHeadOffsetY,
      hf.mHeadOffsetZ,
    });
  const auto anchor = MakeHorizonAlignedAnchor(hmdPose, hf);
  const auto targetMatrix = headLocalMatrix * anchor;
  const Pose targetPose {
    .mPosition = targetMatrix.Translation(),
    .mOrientation = Quaternion::CreateFromRotationMatrix(targetMatrix),
  };

  const bool headOffsetsChanged = state.mInitialized
    && (state.mLastHeadOffsetX != hf.mHeadOffsetX
        || state.mLastHeadOffsetY != hf.mHeadOffsetY
        || state.mLastHeadOffsetZ != hf.mHeadOffsetZ
        || state.mLastHeadPitchDeg != hf.mHeadPitchDeg
        || state.mLastHeadYawDeg != hf.mHeadYawDeg);
  state.mLastHeadOffsetX = hf.mHeadOffsetX;
  state.mLastHeadOffsetY = hf.mHeadOffsetY;
  state.mLastHeadOffsetZ = hf.mHeadOffsetZ;
  state.mLastHeadPitchDeg = hf.mHeadPitchDeg;
  state.mLastHeadYawDeg = hf.mHeadYawDeg;

  if (
    !state.mInitialized || modeChanged || headOffsetsChanged
    || effectiveMode == VRFollowMode::HeadLocked) {
    state.mInitialized = true;
    state.mCurrentPose = targetPose;
    state.mAnchorHmdPosition = hmdPose.mPosition;
    return targetPose;
  }

  if (effectiveMode == VRFollowMode::HeadFollowSmooth) {
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
  const float dtSeconds,
  const std::optional<Pose>& handMountPose) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }

  auto effectiveConfig = config;
  effectiveConfig.mVR = this->GetEffectiveVRSettings(config.mVR);
  const auto now = std::chrono::steady_clock::now();
  const bool dragging =
    mManipulation.mDragMode != ManipulationState::DragMode::None;
  const bool recentOptimistic =
    (now - mManipulation.mLastChangeTime) < std::chrono::milliseconds(450);

  std::vector<Layer> ret;
  ret.reserve(layers.size());
  for (const auto& layerConfig : layers) {
    if (!layerConfig.mVREnabled) {
      continue;
    }

    auto workingConfig = layerConfig;
    if (
      (dragging || recentOptimistic)
      && layerConfig.mLayerID == mManipulation.mTargetLayerID) {
      if (mManipulation.mHasOptimisticPose) {
        workingConfig.mVR.mPose = mManipulation.mOptimisticWorldPose;
      }
      if (
        mManipulation.mHasOptimisticSize
        && mManipulation.mOptimisticSize.x > 0.05f
        && mManipulation.mOptimisticSize.y > 0.05f) {
        workingConfig.mVR.mPhysicalSize = {
          mManipulation.mOptimisticSize.x,
          mManipulation.mOptimisticSize.y,
        };
      }
    }

    auto params = GetRenderParameters(
      effectiveConfig, workingConfig, hmdPose, dtSeconds, handMountPose);
    ret.push_back(Layer {&layerConfig, params});
  }

  if (!effectiveConfig.mVR.mEnableGazeInputFocus) {
    return ret;
  }
  const auto activeLayerID = effectiveConfig.mGlobalInputLayerID;

  const auto activeIt =
    std::ranges::find(ret, activeLayerID, [](const Layer& layer) {
      return layer.mLayerConfig->mLayerID;
    });
  if (
    (activeIt != ret.end())
    && (activeIt->mRenderParameters.mIsLookingAtKneeboard)) {
    return ret;
  }

  for (const auto& [layerConfig, renderParams] :
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
  const float dtSeconds,
  const std::optional<Pose>& handMountPose) {
  const auto kneeboardPose = this->GetKneeboardPose(
    config.mVR, layer, hmdPose, dtSeconds, handMountPose);
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
