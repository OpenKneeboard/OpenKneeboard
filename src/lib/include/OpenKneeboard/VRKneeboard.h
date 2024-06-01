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

#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRConfig.h>

#include <directxtk/SimpleMath.h>

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
    uint64_t mCacheKey;
    float mKneeboardOpacity;
    bool mIsLookingAtKneeboard;
  };

  struct Layer {
    const SHM::LayerConfig* mLayerConfig {nullptr};
    RenderParameters mRenderParameters;
  };

 protected:
  std::vector<Layer> GetLayers(const SHM::Snapshot&, const Pose& hmdPose);

 private:
  RenderParameters GetRenderParameters(
    const SHM::Snapshot&,
    const SHM::LayerConfig&,
    const Pose& hmdPose);

  struct Sizes {
    Vector2 mNormalSize;
    Vector2 mZoomedSize;
  };

  uint64_t mRecenterCount = 0;
  Matrix mRecenter = Matrix::Identity;
  std::unordered_map<uint64_t, bool> mIsLookingAtKneeboard;
  std::optional<float> mEyeHeight;

  Pose GetKneeboardPose(
    const VRRenderConfig& vr,
    const SHM::LayerConfig&,
    const Pose& hmdPose);

  Vector2 GetKneeboardSize(
    const SHM::Config& config,
    const SHM::LayerConfig&,
    bool isLookingAtKneeboard);

  bool IsLookingAtKneeboard(
    const SHM::Config&,
    const SHM::LayerConfig&,
    const Pose& hmdPose,
    const Pose& kneeboardPose);

  Sizes GetSizes(const VRRenderConfig&, const SHM::LayerConfig&) const;

  void MaybeRecenter(const VRRenderConfig& vr, const Pose& hmdPose);
  void Recenter(const VRRenderConfig& vr, const Pose& hmdPose);
};

}// namespace OpenKneeboard
