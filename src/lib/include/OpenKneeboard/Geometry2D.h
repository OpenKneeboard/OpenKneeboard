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

#include <d2d1.h>
#include <d3d11.h>

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
#include <OpenKneeboard/json.h>
#endif

namespace OpenKneeboard::Geometry2D {

template <class T>
struct Size {
  T mWidth {};
  T mHeight {};

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

  constexpr Size<T> ScaledToFit(const Size<T>& container) const noexcept {
    const auto scaleX = static_cast<float>(container.mWidth) / mWidth;
    const auto scaleY = static_cast<float>(container.mHeight) / mHeight;
    const auto scale = std::min(scaleX, scaleY);

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

  template <class TSize, class TValue>
    requires(std::integral<T> || std::floating_point<TValue>)
  constexpr TSize StaticCast() const noexcept {
    return TSize {
      Width<TValue>(),
      Height<TValue>(),
    };
  }

  template <std::integral TValue, class TSize = Size<TValue>>
    requires std::floating_point<T>
  constexpr TSize Rounded() const noexcept {
    return {
      static_cast<TValue>(std::lround(mWidth)),
      static_cast<TValue>(std::lround(mHeight)),
    };
  }

  constexpr operator D2D1_SIZE_U() const {
    return StaticCast<D2D1_SIZE_U, UINT32>();
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

  constexpr auto operator<=>(const Point<T>&) const noexcept = default;

  template <class TPoint, class TValue>
  constexpr TPoint StaticCast() const noexcept {
    return TPoint {
      static_cast<TValue>(mX),
      static_cast<TValue>(mY),
    };
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

  template <class TV = T>
  constexpr auto Left() const {
    return mOffset.X<TV>();
  }

  template <class TV = T>
  constexpr auto Top() const {
    return mOffset.Y<TV>();
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

  template <class TRect, class TValue>
  constexpr TRect StaticCastWithBottomRight() const noexcept {
    return {
      static_cast<TValue>(mOffset.mX),
      static_cast<TValue>(mOffset.mY),
      static_cast<TValue>(mOffset.mX + mSize.mWidth),
      static_cast<TValue>(mOffset.mY + mSize.mHeight),
    };
  }

  template <class TRect, class TValue>
  constexpr TRect StaticCastWithSize() const noexcept {
    return {
      static_cast<TValue>(mOffset.mX),
      static_cast<TValue>(mOffset.mY),
      static_cast<TValue>(mSize.mWidth),
      static_cast<TValue>(mSize.mHeight),
    };
  }

  constexpr operator D3D11_RECT() const {
    return StaticCastWithBottomRight<D3D11_RECT, LONG>();
  }

  constexpr operator D2D_RECT_U() const {
    return StaticCastWithBottomRight<D2D_RECT_U, UINT32>();
  }

  constexpr operator D2D1_RECT_F() const {
    return StaticCastWithBottomRight<D2D1_RECT_F, FLOAT>();
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