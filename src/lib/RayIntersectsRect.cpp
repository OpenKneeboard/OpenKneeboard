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
