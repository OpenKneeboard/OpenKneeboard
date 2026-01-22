// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/ScalingKind.hpp>

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
        p.mLength *=
          (static_cast<float>(ret.mPixelSize.mHeight) / mPixelSize.mHeight);
        break;
      case Direction::Horizontal:
        p.mLength *=
          (static_cast<float>(ret.mPixelSize.mWidth) / mPixelSize.mWidth);
        break;
      case Direction::Diagonal: {
        const auto aspectRatio =
          static_cast<float>(mPixelSize.mWidth) / mPixelSize.mHeight;
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
