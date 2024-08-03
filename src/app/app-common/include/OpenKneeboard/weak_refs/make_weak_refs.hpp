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

#include "concepts.hpp"
#include "lock_weak_ref.hpp"
#include "make_weak_ref.hpp"
#include "ref_types.hpp"

#include <optional>
#include <tuple>

namespace OpenKneeboard::weak_refs {

template <convertible_to_weak_ref... Args>
constexpr auto make_weak_refs(Args&&... args) noexcept {
  return std::make_tuple(make_weak_ref(std::forward<Args>(args))...);
}

template <weak_ref... Weak>
constexpr auto lock_weak_refs(const std::tuple<Weak...>& weak) noexcept {
  static_assert((strong_ref<strong_ref_t<Weak>> && ...));
  using strong_t = std::tuple<strong_ref_t<Weak>...>;
  using locked_t = std::optional<strong_t>;

  const auto strong = std::apply(
    []<class... Args>(Args&&... args) constexpr -> strong_t {
      return std::make_tuple(lock_weak_ref(std::forward<Args>(args))...);
    },
    weak);
  const auto all_live = std::apply(
    []<strong_ref... Args>(Args&&... args) constexpr -> bool {
      return (static_cast<bool>(std::forward<Args>(args)) && ...);
    },
    strong);
  if (!all_live) {
    return locked_t {std::nullopt};
  }
  return locked_t {strong};
}

}// namespace OpenKneeboard::weak_refs