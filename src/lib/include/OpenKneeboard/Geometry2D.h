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

#include <array>
#include <cmath>
#include <compare>
#include <concepts>

#include <d2d1.h>
#include <d3d11.h>

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
#include <OpenKneeboard/json.h>
#endif

namespace OpenKneeboard::Geometry2D {

enum class ScaleToFitMode {
  ShrinkOrGrow,
  GrowOnly,
  ShrinkOnly,
};

template <class T>
struct Size {
  T mWidth {};
  T mHeight {};

  constexpr operator bool() const noexcept {
    return mWidth >= 1 && mHeight >= 1;
  }

  constexpr Size<T> operator/(const T divisor) const noexcept {
    return {mWidth / divisor, mHeight / divisor};
  }

  constexpr Size<T> operator*(const std::integral auto operand) const noexcept {
    return {mWidth * operand, mHeight * operand};
  }

  constexpr Size<T> operator*(
    const std::floating_point auto operand) const noexcept
    requires std::floating_point<T>
  {
    return {mWidth * operand, mHeight * operand};
  }

  constexpr auto operator<=>(const Size<T>&) const noexcept = default;

  constexpr Size() {
  }

  constexpr Size(T width, T height) : mWidth(width), mHeight(height) {
  }

  template <class TV = T>
  constexpr auto Width() const noexcept {
    return static_cast<TV>(mWidth);
  }

  template <class TV = T>
  constexpr auto Height() const noexcept {
    return static_cast<TV>(mHeight);
  }

  constexpr Size<T> ScaledToFit(
    const Size<T>& container,
    ScaleToFitMode mode = ScaleToFitMode::ShrinkOrGrow) const noexcept {
    const auto scaleX = static_cast<float>(container.mWidth) / mWidth;
    const auto scaleY = static_cast<float>(container.mHeight) / mHeight;
    const auto scale = std::min(scaleX, scaleY);

    if (scale > 1 && mode == ScaleToFitMode::ShrinkOnly) {
      return *this;
    }

    if (scale < 1 && mode == ScaleToFitMode::GrowOnly) {
      return *this;
    }

    const Size<float> scaled {
      mWidth * scale,
      mHeight * scale,
    };
    if constexpr (std::integral<T>) {
      return scaled.Rounded<T>();
    } else {
      return scaled;
    }
  }

  constexpr Size<T> IntegerScaledToFit(
    const Size<T>& container,
    ScaleToFitMode mode = ScaleToFitMode::ShrinkOrGrow) const noexcept {
    const auto scaleX = static_cast<float>(container.mWidth) / mWidth;
    const auto scaleY = static_cast<float>(container.mHeight) / mHeight;
    const auto scale = std::min(scaleX, scaleY);

    if (scale > 1) {
      if (mode == ScaleToFitMode::ShrinkOnly) {
        return *this;
      }
      const auto mult = static_cast<uint32_t>(std::floor(scale));
      return {
        mWidth * mult,
        mHeight * mult,
      };
    }

    if (mode == ScaleToFitMode::GrowOnly) {
      return *this;
    }

    const auto divisor = static_cast<uint32_t>(std::ceil(1 / scale));
    return {
      mWidth / divisor,
      mHeight / divisor,
    };
  }

  template <class TValue, class TSize = Size<TValue>>
    requires(std::integral<T> || std::floating_point<TValue>)
  constexpr TSize StaticCast() const noexcept {
    return TSize {
      Width<TValue>(),
      Height<TValue>(),
    };
  }

  template <std::integral TValue, class TSize = Size<TValue>>
  constexpr TSize Floor() const noexcept {
    return TSize {
      static_cast<TValue>(std::floor(mWidth)),
      static_cast<TValue>(std::floor(mHeight)),
    };
  }

  template <class TValue, class TSize = Size<TValue>>
    requires std::floating_point<T>
  constexpr TSize Rounded() const noexcept {
    return {
      static_cast<TValue>(std::lround(mWidth)),
      static_cast<TValue>(std::lround(mHeight)),
    };
  }

  constexpr operator D2D1_SIZE_U() const
    requires std::integral<T>
  {
    return StaticCast<UINT32, D2D1_SIZE_U>();
  }

  constexpr operator D2D1_SIZE_F() const {
    return StaticCast<FLOAT, D2D1_SIZE_F>();
  }
};

template <class T>
struct Point {
  T mX {};
  T mY {};

  template <class TV = T>
  constexpr auto X() const {
    return static_cast<TV>(mX);
  }

  template <class TV = T>
  constexpr auto Y() const {
    return static_cast<TV>(mY);
  }

  constexpr Point<T> operator/(const T divisor) const noexcept {
    return {mX / divisor, mY / divisor};
  }

  constexpr Point<T> operator*(const T operand) const noexcept {
    return {mX * operand, mY * operand};
  }

  constexpr Point<T>& operator+=(const Point<T>& operand) noexcept {
    mX += operand.mX;
    mY += operand.mY;
    return *this;
  }

  friend constexpr Point<T> operator+(
    Point<T> lhs,
    const Point<T>& rhs) noexcept {
    lhs += rhs;
    return lhs;
  }

  constexpr auto operator<=>(const Point<T>&) const noexcept = default;

  template <class TValue, class TPoint = Point<TValue>>
  constexpr TPoint StaticCast() const noexcept {
    return TPoint {
      static_cast<TValue>(mX),
      static_cast<TValue>(mY),
    };
  }

  template <class TValue, class TPoint = Point<TValue>>
    requires std::floating_point<T>
  constexpr TPoint Rounded() const noexcept {
    return {
      static_cast<TValue>(std::lround(mX)),
      static_cast<TValue>(std::lround(mY)),
    };
  }

  constexpr operator D2D1_POINT_2F() const noexcept {
    return StaticCast<FLOAT, D2D1_POINT_2F>();
  }

  constexpr operator D2D1_POINT_2U() const noexcept
    requires std::integral<T>
  {
    return StaticCast<UINT32, D2D1_POINT_2U>();
  }
};

template <class T>
struct Rect {
  enum class Origin {
    TopLeft,
    BottomLeft,
  };

  Point<T> mOffset {};
  Size<T> mSize {};

  Origin mOrigin {Origin::TopLeft};

  constexpr operator bool() const noexcept {
    return mSize;
  }

  constexpr Rect<T> operator/(const T divisor) const noexcept {
    return {
      mOffset / divisor,
      mSize / divisor,
    };
  }

  template <std::integral TValue>
  constexpr Rect<T> operator*(const TValue operand) const noexcept {
    return {
      mOffset * operand,
      mSize * operand,
    };
  }

  template <std::floating_point TValue>
    requires std::floating_point<T>
  constexpr Rect<T> operator*(const TValue operand) const noexcept {
    return {
      mOffset * operand,
      mSize * operand,
    };
  }

  template <class TV = T>
  constexpr auto Left() const {
    return mOffset.template X<TV>();
  }

  template <class TV = T>
  constexpr auto Top() const {
    return mOffset.template Y<TV>();
  }

  template <class TV = T>
  constexpr auto Right() const {
    return Left<TV>() + mSize.mWidth;
  }

  template <class TV = T>
  constexpr auto Bottom() const {
    if (mOrigin == Origin::TopLeft) {
      return Top<TV>() + mSize.mHeight;
    }
    return Top<TV>() - mSize.mHeight;
  }

  constexpr Point<T> TopLeft() const {
    return mOffset;
  }

  constexpr Point<T> BottomRight() const {
    return {Right(), Bottom()};
  }

  template <class TV = T>
  constexpr auto Width() const noexcept {
    return mSize.template Width<TV>();
  }

  template <class TV = T>
  constexpr auto Height() const noexcept {
    return mSize.template Height<TV>();
  }

  constexpr Rect<T> WithOrigin(Origin origin, const Size<T>& container) const {
    if (origin == mOrigin) {
      return *this;
    }
    return {
      {mOffset.mX, container.mHeight - mOffset.mY},
      mSize,
      origin,
    };
  }

  constexpr auto operator<=>(const Rect<T>&) const noexcept = default;

  template <class TValue, class TRect = Rect<TValue>>
  constexpr TRect StaticCast() const noexcept {
    return {
      {
        static_cast<TValue>(mOffset.mX),
        static_cast<TValue>(mOffset.mY),
      },
      {
        static_cast<TValue>(mSize.mWidth),
        static_cast<TValue>(mSize.mHeight),
      },
    };
  }

  template <class TValue, class TRect>
  constexpr TRect StaticCastWithBottomRight() const noexcept {
    return {
      static_cast<TValue>(mOffset.mX),
      static_cast<TValue>(mOffset.mY),
      static_cast<TValue>(mOffset.mX + mSize.mWidth),
      static_cast<TValue>(mOffset.mY + mSize.mHeight),
    };
  }

  template <class TValue>
    requires std::floating_point<T>
  constexpr Rect<TValue> Rounded() const noexcept {
    return {
      mOffset.template Rounded<TValue>(),
      mSize.template Rounded<TValue>(),
    };
  }

  constexpr operator D3D11_RECT() const
    requires std::integral<T>
  {
    return StaticCastWithBottomRight<LONG, D3D11_RECT>();
  }

  constexpr operator D2D_RECT_U() const
    requires std::integral<T>
  {
    return StaticCastWithBottomRight<UINT32, D2D_RECT_U>();
  }

  constexpr operator D2D1_RECT_F() const {
    return StaticCastWithBottomRight<FLOAT, D2D1_RECT_F>();
  }
};

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
template <class T>
void from_json(const nlohmann::json& j, Size<T>& v) {
  v.mWidth = j.at("Width");
  v.mHeight = j.at("Height");
}
template <class T>
void to_json(nlohmann::json& j, const Size<T>& v) {
  j["Width"] = v.mWidth;
  j["Height"] = v.mHeight;
}
#endif

}// namespace OpenKneeboard::Geometry2D