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
#include <string_view>

namespace OpenKneeboard {
template <size_t N>
struct StringTemplateParameter {
  StringTemplateParameter() = delete;
  consteval StringTemplateParameter(char const (&init)[N]) {
    std::ranges::copy(init, value);
  }

  char value[N] {};

  constexpr operator std::string_view() const noexcept {
    return std::string_view {value};
  }
};

template <>
struct StringTemplateParameter<0> {};

template <StringTemplateParameter T>
consteval auto operator""_tp() {
  return T;
}
}// namespace OpenKneeboard
