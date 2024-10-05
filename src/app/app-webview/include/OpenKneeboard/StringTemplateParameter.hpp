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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#pragma once

#include <algorithm>
#include <ranges>
#include <string_view>

namespace OpenKneeboard {
template <size_t N>
struct StringTemplateParameter {
  explicit consteval StringTemplateParameter() = default;

  consteval StringTemplateParameter(char const (&init)[N]) {
    std::ranges::copy(init, value);
  }

  char value[N] {};

  constexpr operator std::string_view() const noexcept {
    return std::string_view {value, N - 1};
  }

  static constexpr auto size() noexcept {
    return N - 1;// null terminator
  }

  template <size_t M>
  consteval auto operator+(StringTemplateParameter<M> rhs) const noexcept {
    // Both inputs are null-terminated, but we only need one null-terminator
    StringTemplateParameter<N + M - 1> ret;
    std::ranges::copy(value, ret.value);
    std::ranges::copy(rhs.value, ret.value + N - 1);
    return ret;
  }

  constexpr bool operator==(const StringTemplateParameter<N>&) const noexcept
    = default;
  template <size_t M>
    requires(N != M)
  constexpr bool operator==(const StringTemplateParameter<M>&) const noexcept {
    return false;
  }
};

template <>
struct StringTemplateParameter<0> {};

template <StringTemplateParameter T>
consteval auto operator""_tp() {
  return T;
}

static_assert("foo"_tp == "foo"_tp);
static_assert("foo"_tp != "bar"_tp);
static_assert(("foo"_tp + "bar"_tp) == "foobar"_tp);

}// namespace OpenKneeboard
