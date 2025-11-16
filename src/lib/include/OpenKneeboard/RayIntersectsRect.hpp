// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <directxtk/SimpleMath.h>

namespace OpenKneeboard {

bool RayIntersectsRect(
  const DirectX::SimpleMath::Vector3& rayOrigin,
  const DirectX::SimpleMath::Quaternion& rayOrientation,
  const DirectX::SimpleMath::Vector3& rectCenter,
  const DirectX::SimpleMath::Quaternion& rectOrientation,
  const DirectX::SimpleMath::Vector2& rectSize);

}
