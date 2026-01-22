// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <concepts>
#include <type_traits>

namespace OpenKneeboard {

/// Specialize this for your enum class to enable operator overloads
template <class T>
  requires std::is_enum_v<T>
  && std::unsigned_integral<std::underlying_type_t<T>>
constexpr bool is_bitflags_v = false;

namespace detail {

template <class T>
  requires std::is_enum_v<T>
  && std::unsigned_integral<std::underlying_type_t<T>>
consteval bool supports_bitflags_adl(T) {
  if constexpr (requires(T v) { supports_bitflags(v); }) {
    return supports_bitflags(T {});
  } else {
    return is_bitflags_v<T>;
  }
}
}// namespace detail

template <class T>
concept bitflags = std::is_enum_v<T> && detail::supports_bitflags_adl(T {});

template <bitflags T>
constexpr T operator|(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) | static_cast<UT>(b));
}

template <bitflags T>
constexpr T operator&(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) & static_cast<UT>(b));
}

template <bitflags T>
constexpr T operator^(T a, T b) = delete;

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

}// namespace OpenKneeboard
