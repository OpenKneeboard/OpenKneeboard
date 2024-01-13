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

#include <OpenKneeboard/Pixels.h>
#include <OpenKneeboard/ScalingKind.h>

#include <cmath>
#include <optional>

namespace OpenKneeboard {
struct PhysicalSize {
  enum class Direction {
    Horizontal,
    Vertical,
    Diagonal,
  };

  Direction mDirection {Direction::Diagonal};
  /// Meters
  float mLength {};
};

struct PreferredSize {
  PixelSize mPixelSize {};
  ScalingKind mScalingKind {ScalingKind::Bitmap};
  std::optional<PhysicalSize> mPhysicalSize;

  constexpr PreferredSize Extended(const PixelSize& extension) const {
    auto ret = *this;
    ret.mPixelSize.mWidth += extension.mWidth;
    ret.mPixelSize.mHeight += extension.mHeight;

    if (!mPhysicalSize) {
      return ret;
    }

    auto& p = *ret.mPhysicalSize;
    using Direction = PhysicalSize::Direction;
    switch (p.mDirection) {
      case Direction::Vertical:
        p.mLength
          *= (static_cast<float>(ret.mPixelSize.mHeight) / mPixelSize.mHeight);
        break;
      case Direction::Horizontal:
        p.mLength
          *= (static_cast<float>(ret.mPixelSize.mWidth) / mPixelSize.mWidth);
        break;
      case Direction::Diagonal: {
        const auto aspectRatio
          = static_cast<float>(mPixelSize.mWidth) / mPixelSize.mHeight;
        const auto height = std::sqrtf(
          (p.mLength * p.mLength) / ((aspectRatio * aspectRatio) + 1));
        p.mLength = height
          * (static_cast<float>(ret.mPixelSize.mHeight) / mPixelSize.mHeight);
        p.mDirection = Direction::Vertical;
        break;
      }
    }

    return ret;
  }
};

}// namespace OpenKneeboard