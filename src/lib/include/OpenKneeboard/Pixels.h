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
#include <initializer_list>

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

  template <class T = decltype(mWidth)>
  constexpr auto GetWidth() const noexcept {
    return static_cast<T>(mWidth);
  }

  template <class T = decltype(mHeight)>
  constexpr auto GetHeight() const noexcept {
    return static_cast<T>(mHeight);
  }

  constexpr operator D2D1_SIZE_U() const noexcept {
    return {mWidth, mHeight};
  }

  constexpr PixelSize ScaledToFit(const PixelSize& container) const noexcept {
    const auto scaleX = static_cast<float>(container.mWidth) / mWidth;
    const auto scaleY = static_cast<float>(container.mHeight) / mHeight;
    const auto scale = std::min(scaleX, scaleY);
    return {
      static_cast<uint32_t>(std::lround(mWidth * scale)),
      static_cast<uint32_t>(std::lround(mHeight * scale)),
    };
  }

  template <class TSize, class TValue>
  constexpr TSize StaticCast() const noexcept {
    return TSize {GetWidth<TValue>(), GetHeight<TValue>()};
  }
};

struct PixelPoint {
  uint32_t mX {};
  uint32_t mY {};

  constexpr auto operator<=>(const PixelPoint&) const noexcept = default;

  template <class TPoint, class TValue>
  constexpr TPoint StaticCast() const noexcept {
    return TPoint {
      static_cast<TValue>(mX),
      static_cast<TValue>(mY),
    };
  }
};

struct PixelRect {
  PixelPoint mOrigin {};
  PixelSize mSize {};

  constexpr auto operator<=>(const PixelRect&) const noexcept = default;

  constexpr operator RECT() const noexcept {
    return StaticCastWithBottomRight<RECT, LONG>();
  }

  constexpr operator D2D_RECT_F() const noexcept {
    return StaticCastWithBottomRight<D2D_RECT_F, FLOAT>();
  };

  constexpr operator D2D_RECT_U() const noexcept {
    return StaticCastWithBottomRight<D2D_RECT_U, UINT32>();
  };

 private:
  template <class TRect, class TValue>
  constexpr TRect StaticCastWithBottomRight() const noexcept {
    return {
      static_cast<TValue>(mOrigin.mX),
      static_cast<TValue>(mOrigin.mY),
      static_cast<TValue>(mOrigin.mX + mSize.mWidth),
      static_cast<TValue>(mOrigin.mY + mSize.mHeight),
    };
  }
};

}// namespace OpenKneeboard