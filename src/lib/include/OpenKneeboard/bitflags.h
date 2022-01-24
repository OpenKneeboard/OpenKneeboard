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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include <concepts>
#include <type_traits>

namespace OpenKneeboard {

/// Specialize this for your enum class to enable operator overloads
template <class T>
  requires std::is_enum_v<T> && std::unsigned_integral<std::underlying_type_t<T>>
constexpr bool is_bitflags_v = false;

template <class T>
concept bitflags = is_bitflags_v<T>;

/// Helper to allow (flags & Foo::Bar) pattern
template <bitflags T>
class CoerceableBitflags final {
 private:
  T value;

 public:
  constexpr CoerceableBitflags(T v) : value(v) {
  }

  constexpr operator T() const {
    return value;
  }

  constexpr operator bool() const {
    return static_cast<bool>(value);
  }
};

template <bitflags T>
constexpr T operator|(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) | static_cast<UT>(b));
}

template <bitflags T>
constexpr CoerceableBitflags<T> operator&(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) & static_cast<UT>(b));
}

template <bitflags T>
constexpr T operator^(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) ^ static_cast<UT>(b));
}

template <bitflags T>
constexpr T operator~(T a) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(~static_cast<UT>(a));
}

template <bitflags T>
constexpr T& operator|=(T& a, const T& b) {
  return a = (a | b);
}

template <bitflags T>
constexpr T& operator&=(T& a, const T& b) {
  return a = (a & b);
}

template <bitflags T>
constexpr T& operator^=(T& a, const T& b) {
  return a = (a ^ b);
}

}// namespace OpenKneeboard
