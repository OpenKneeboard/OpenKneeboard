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
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRConfig.h>

namespace OpenKneeboard {

class VRKneeboard {
 public:
  using Matrix = DirectX::SimpleMath::Matrix;
  using Quaternion = DirectX::SimpleMath::Quaternion;
  using Vector2 = DirectX::SimpleMath::Vector2;
  using Vector3 = DirectX::SimpleMath::Vector3;

  struct Pose {
    Vector3 mPosition {};
    Quaternion mOrientation {};
  };

  struct RenderParameters {
    Pose mKneeboardPose;
    Vector2 mKneeboardSize;
    float mKneeboardOpacity;
  };

 protected:
  enum class YOrigin {
    FLOOR_LEVEL,
    EYE_LEVEL,
  };

  virtual YOrigin GetYOrigin() = 0;

  RenderParameters GetRenderParameters(const SHM::Config&, const Pose& hmdPose);

 private:
  struct Sizes {
    Vector2 mNormalSize;
    Vector2 mZoomedSize;
  };

  uint64_t mRecenterCount = 0;
  Matrix mRecenter = Matrix::Identity;

  Pose GetKneeboardPose(const VRRenderConfig& vr, const Pose& hmdPose);

  Vector2 GetKneeboardSize(
    const SHM::Config& config,
    bool isLookingAtKneeboard);

  bool IsLookingAtKneeboard(
    const SHM::Config& config,
    const Pose& hmdPose,
    const Pose& kneeboardPose);

  bool IsGazeEnabled(const VRRenderConfig& vr);
  Sizes GetSizes(const SHM::Config& config) const;

  void Recenter(const VRRenderConfig& vr, const Pose& hmdPose);
};

}// namespace OpenKneeboard
