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

#include <cmath>
#include <compare>
#include <cstdint>

#include <d2d1.h>

namespace OpenKneeboard {
struct PixelSize {
  uint32_t mWidth {};
  uint32_t mHeight {};

  constexpr auto operator<=>(const PixelSize&) const noexcept = default;

  constexpr PixelSize() {
  }

  constexpr PixelSize(uint32_t width, uint32_t height)
    : mWidth(width), mHeight(height) {
  }

  constexpr PixelSize(const D2D1_SIZE_U& d2d)
    : PixelSize(d2d.width, d2d.height) {
  }

  constexpr operator D2D1_SIZE_U() const {
    return {mWidth, mHeight};
  }

  constexpr PixelSize ScaledToFit(const PixelSize& container) const {
    const auto scaleX = static_cast<float>(container.mWidth) / mWidth;
    const auto scaleY = static_cast<float>(container.mHeight) / mHeight;
    const auto scale = std::min(scaleX, scaleY);
    return {
      static_cast<uint32_t>(std::lround(mWidth * scale)),
      static_cast<uint32_t>(std::lround(mHeight * scale)),
    };
  }
};

struct PixelPoint {
  uint32_t mX {};
  uint32_t mY {};

  constexpr auto operator<=>(const PixelPoint&) const noexcept = default;
};

struct PixelRect {
  PixelPoint mOrigin {};
  PixelSize mSize {};

  constexpr auto operator<=>(const PixelRect&) const noexcept = default;
};

}// namespace OpenKneeboard