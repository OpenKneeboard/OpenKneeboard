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
