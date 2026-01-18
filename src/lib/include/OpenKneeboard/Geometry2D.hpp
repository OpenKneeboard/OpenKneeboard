// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <d3d11.h>

#include <felly/numeric_cast.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <concepts>

#include <d2d1.h>

namespace OpenKneeboard::Geometry2D {

using namespace felly::numeric_cast_types;

enum class ScaleToFitMode {
  ShrinkOrGrow,
  GrowOnly,
  ShrinkOnly,
};

template <class T>
struct Size {
  T mWidth {};
  T mHeight {};

  constexpr operator bool() const noexcept
    requires std::integral<T>
  {
    return !IsEmpty();
  }

  constexpr bool IsEmpty() const noexcept
    requires std::integral<T>
  {
    return mWidth == 0 || mHeight == 0;
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

  constexpr bool operator==(const Size<T>&) const noexcept = default;

  constexpr Size() {
  }

  constexpr Size(T width, T height) : mWidth(width), mHeight(height) {
  }

  template <class TV = T>
  constexpr auto Width() const noexcept {
    return numeric_cast<TV>(mWidth);
  }

  template <class TV = T>
  constexpr auto Height() const noexcept {
    return numeric_cast<TV>(mHeight);
  }

  constexpr Size<T> ScaledToFit(
    const Size<T>& container,
    ScaleToFitMode mode = ScaleToFitMode::ShrinkOrGrow) const noexcept {
    const auto scaleX = numeric_cast<float>(container.mWidth) / mWidth;
    const auto scaleY = numeric_cast<float>(container.mHeight) / mHeight;
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
    const auto scaleX = numeric_cast<float>(container.mWidth) / mWidth;
    const auto scaleY = numeric_cast<float>(container.mHeight) / mHeight;
    const auto scale = std::min(scaleX, scaleY);

    if (scale > 1) {
      if (mode == ScaleToFitMode::ShrinkOnly) {
        return *this;
      }
      const auto mult = numeric_cast<uint32_t>(std::floor(scale));
      return {
        mWidth * mult,
        mHeight * mult,
      };
    }

    if (mode == ScaleToFitMode::GrowOnly) {
      return *this;
    }

    const auto divisor = numeric_cast<uint32_t>(std::ceil(1 / scale));
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
      numeric_cast<TValue>(std::floor(mWidth)),
      numeric_cast<TValue>(std::floor(mHeight)),
    };
  }

  template <class TValue, class TSize = Size<TValue>>
    requires std::floating_point<T>
  constexpr TSize Rounded() const noexcept {
    return {
      numeric_cast<TValue>(std::lround(mWidth)),
      numeric_cast<TValue>(std::lround(mHeight)),
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
    return numeric_cast<TV>(mX);
  }

  template <class TV = T>
  constexpr auto Y() const {
    return numeric_cast<TV>(mY);
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

  constexpr Point<T> operator-() const noexcept {
    return (*this) * -1;
  };

  constexpr auto operator-(const Point<T>& other) const noexcept {
    return *this + (-other);
  }

  constexpr bool operator==(const Point<T>&) const noexcept = default;

  template <class TValue, class TPoint = Point<TValue>>
  constexpr TPoint StaticCast() const noexcept {
    return TPoint {
      numeric_cast<TValue>(mX),
      numeric_cast<TValue>(mY),
    };
  }

  template <class TValue, class TPoint = Point<TValue>>
    requires std::floating_point<T>
  constexpr TPoint Rounded() const noexcept {
    return {
      numeric_cast<TValue>(std::lround(mX)),
      numeric_cast<TValue>(std::lround(mY)),
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

  constexpr Rect<T> Clamped(const Rect<T>& container) const {
    const Point<T> topLeft {
      std::clamp(Left(), container.Left(), container.Right()),
      std::clamp(Top(), container.Top(), container.Bottom()),
    };
    const Size<T> size {
      std::clamp(Width(), T {0}, container.Width() - topLeft.X()),
      std::clamp(Height(), T {0}, container.Height() - topLeft.Y()),
    };
    return {topLeft, size};
  }

  /// Convenience method for containers anchored at {0 , 0}
  constexpr Rect<T> Clamped(const Size<T>& containerSize) const {
    return Clamped({Point<T> {}, containerSize});
  }

  constexpr bool operator==(const Rect<T>&) const noexcept = default;

  template <class TValue, class TRect = Rect<TValue>>
  constexpr TRect StaticCast() const noexcept {
    return {
      {
        numeric_cast<TValue>(mOffset.mX),
        numeric_cast<TValue>(mOffset.mY),
      },
      {
        numeric_cast<TValue>(mSize.mWidth),
        numeric_cast<TValue>(mSize.mHeight),
      },
    };
  }

  template <class TValue, class TRect>
  constexpr TRect StaticCastWithBottomRight() const noexcept {
    return {
      numeric_cast<TValue>(mOffset.mX),
      numeric_cast<TValue>(mOffset.mY),
      numeric_cast<TValue>(mOffset.mX + mSize.mWidth),
      numeric_cast<TValue>(mOffset.mY + mSize.mHeight),
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

}// namespace OpenKneeboard::Geometry2D