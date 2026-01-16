// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <concepts>
#include <format>
#include <stdexcept>
#include <utility>

namespace OpenKneeboard {

struct numeric_cast_range_error : std::range_error {
  using range_error::range_error;

  template <class T>
  numeric_cast_range_error(std::type_identity<T>, const auto value)
    : std::range_error(
        std::format(
          "Value {} out of range {}..{}",
          value,
          std::numeric_limits<T>::lowest(),
          std::numeric_limits<T>::max())) {
  }
};

template <std::integral T>
[[nodiscard]]
constexpr T numeric_cast(const std::integral auto v) {
  if (!std::in_range<T>(v)) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, v);
  }
  return static_cast<T>(v);
}

template <std::floating_point T, std::floating_point U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  if (std::isnan(u)) {
    return static_cast<T>(u);
  }

  using V = std::common_type_t<T, U>;
  const auto v = static_cast<V>(u);

  constexpr auto Lowest = static_cast<V>(std::numeric_limits<T>::lowest());
  constexpr auto Max = static_cast<V>(std::numeric_limits<T>::max());

  if (v < Lowest || v > Max) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, v);
  }
  return static_cast<T>(v);
}

template <std::floating_point T, std::integral U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  constexpr auto Lowest = std::numeric_limits<T>::lowest();
  constexpr auto Max = std::numeric_limits<T>::max();
  if (u < Lowest || u > Max) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, u);
  }
  return static_cast<T>(u);
}

template <std::integral T, std::floating_point U>
[[nodiscard]]
constexpr T numeric_cast(const U u) {
  if (std::isnan(u)) [[unlikely]] {
    throw numeric_cast_range_error("Can't convert NaN to an integral type");
  }

  constexpr auto Lowest = static_cast<U>(std::numeric_limits<T>::lowest());
  constexpr auto Max = static_cast<U>(std::numeric_limits<T>::max());

  const auto truncated = std::trunc(u);
  if (truncated < Lowest || truncated > Max) [[unlikely]] {
    throw numeric_cast_range_error(std::type_identity<T> {}, u);
  }
  return static_cast<T>(u);
}

}// namespace OpenKneeboard