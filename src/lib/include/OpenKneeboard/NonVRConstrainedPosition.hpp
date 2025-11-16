// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Alignment.hpp>
#include <OpenKneeboard/Pixels.hpp>

#include <compare>
#include <cstdint>
#include <numbers>

namespace OpenKneeboard {

struct NonVRConstrainedPosition {
  uint8_t mHeightPercent = 60;
  uint32_t mPaddingPixels = 10;

  Alignment::Horizontal mHorizontalAlignment = Alignment::Horizontal::Right;
  Alignment::Vertical mVerticalAlignment = Alignment::Vertical::Middle;

  PixelRect Layout(PixelSize canvasSize, PixelSize imageSize) const;

  constexpr bool operator==(const NonVRConstrainedPosition&) const = default;
};
static_assert(std::is_standard_layout_v<NonVRConstrainedPosition>);

}// namespace OpenKneeboard
