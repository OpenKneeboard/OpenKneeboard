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
#include <tuple>

namespace OpenKneeboard {

// concatenate multiple arrays - requires C++20
//
// The usual approach is to default-construct `std::array<T, Na + Nb>`, then
// overwrite the elements, e.g. with `std::copy()` or loops.
//
// The problem with that is it requires that `T` is default-constructable;
// this implementation does not have that requirement.
template <class T, size_t Na, size_t Nb>
constexpr auto array_cat(
  const std::array<T, Na>& a,
  const std::array<T, Nb>& b,
  auto&&... rest) {
  if constexpr (sizeof...(rest) == 0) {
    return std::apply(
      [](auto&&... elements) {
        return std::array {std::forward<decltype(elements)>(elements)...};
      },
      std::tuple_cat(a, b));
  } else {
    return array_cat(a, array_cat(b, std::forward<decltype(rest)>(rest)...));
  }
}

static_assert(
  array_cat(std::array {1, 2}, std::array {34, 5}, std::array {678, 910})
  == std::array {1, 2, 34, 5, 678, 910});

}