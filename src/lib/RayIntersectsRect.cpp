// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/RayIntersectsRect.hpp>

using namespace DirectX::SimpleMath;

namespace OpenKneeboard {

bool RayIntersectsRect(
  const Vector3& rayOrigin,
  const Quaternion& rayOrientation,
  const Vector3& rectCenter,
  const Quaternion& rectOrientation,
  const Vector2& rectSize) {
  const Vector3 rayNormal(Vector3::Transform(Vector3::Forward, rayOrientation));
  const Ray ray(rayOrigin, rayNormal);

  const Plane plane(
    rectCenter, Vector3::Transform(Vector3::Backward, rectOrientation));

  // Does the ray intersect the infinite plane?
  float rayLength = 0;
  if (!ray.Intersects(plane, rayLength)) {
    return false;
  }

  // Where does it intersect the infinite plane?
  const auto worldPoint = rayOrigin + (rayNormal * rayLength);

  // Is that point within the rectangle?
  const auto point = worldPoint - rectCenter;

  const auto x = point.Dot(Vector3::Transform(Vector3::UnitX, rectOrientation));
  if (abs(x) > rectSize.x / 2) {
    return false;
  }

  const auto y = point.Dot(Vector3::Transform(Vector3::UnitY, rectOrientation));
  if (abs(y) > rectSize.y / 2) {
    return false;
  }

  return true;
}

}// namespace OpenKneeboard
