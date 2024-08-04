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

#include "bind_maybe_refs_front.hpp"
#include "bind_refs_front.hpp"

namespace OpenKneeboard::weak_refs::bind_front_adl {

// Marker for tag-based dispatch
struct maybe_refs_t {};
struct only_refs_t {};

namespace {
constexpr auto& maybe_refs = weak_refs_detail::static_const<maybe_refs_t>;
constexpr auto& only_refs = weak_refs_detail::static_const<only_refs_t>;
}// namespace
}// namespace OpenKneeboard::weak_refs::bind_front_adl

namespace OpenKneeboard::weak_refs::bind_front_adl_definitions {
template <class T>
constexpr bool is_adl_tag_v = false;
template <>
constexpr bool is_adl_tag_v<bind_front_adl::maybe_refs_t> = true;
template <>
constexpr bool is_adl_tag_v<bind_front_adl::only_refs_t> = true;

template <class First, class... Rest>
  requires(!is_adl_tag_v<std::decay_t<First>>)
auto adl_bind_front(First&& first, Rest&&... rest) {
  using namespace std;
  return bind_front(std::forward<First>(first), std::forward<Rest>(rest)...);
}

template <class... Args>
auto adl_bind_front(bind_front_adl::maybe_refs_t, Args&&... args) {
  return bind_maybe_refs_front(std::forward<Args>(args)...);
}

template <class... Args>
auto adl_bind_front(bind_front_adl::only_refs_t, Args&&... args) {
  return bind_refs_front(std::forward<Args>(args)...);
}

}// namespace OpenKneeboard::weak_refs::bind_front_adl_definitions

namespace OpenKneeboard::weak_refs::bind_front_adl_detail {
struct bind_front_fn {
  template <class... Args>
  constexpr auto operator()(Args&&... args) const {
    return bind_front_adl_definitions::adl_bind_front(
      std::forward<Args>(args)...);
  }
};

}// namespace OpenKneeboard::weak_refs::bind_front_adl_detail
namespace OpenKneeboard::weak_refs::bind_front_adl {
namespace {
constexpr auto& bind_front
  = weak_refs_detail::static_const<bind_front_adl_detail::bind_front_fn>;
}// namespace
}// namespace OpenKneeboard::weak_refs::bind_front_adl