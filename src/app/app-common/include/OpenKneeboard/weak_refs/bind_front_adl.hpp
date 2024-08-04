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

namespace OpenKneeboard::weak_refs::bind_front_adl_detail {

template <class... TArgs>
concept has_adl_bind_front
  = requires(TArgs... args) { adl_bind_front(std::forward<TArgs>(args)...); };

struct bind_front_fn {
  template <class TFn>
  constexpr auto operator()(TFn&& fn) const noexcept {
    return std::forward<TFn>(fn);
  }

  template <class TFn, class TFirst, class... TRest>
    requires has_adl_bind_front<TFirst, TFn, TRest...>
  constexpr auto operator()(TFn&& fn, TFirst&& first, TRest&&... rest)
    const noexcept {
    return adl_bind_front(
      std::forward<TFirst>(first),
      std::forward<TFn>(fn),
      std::forward<TRest>(rest)...);
  }

  template <class... TArgs>
    requires(sizeof...(TArgs) >= 2 && !has_adl_bind_front<TArgs...>)
  constexpr auto operator()(TArgs&&... args) const noexcept {
    return std::bind_front(std::forward<TArgs>(args)...);
  }
};

}// namespace OpenKneeboard::weak_refs::bind_front_adl_detail

namespace OpenKneeboard::weak_refs::bind_front_adl {

namespace {
constexpr auto const& bind_front
  = weak_refs_detail::static_const<bind_front_adl_detail::bind_front_fn>;
}

}// namespace OpenKneeboard::weak_refs::bind_front_adl

namespace OpenKneeboard::weak_refs::bind_front_adl {

// Marker for tag-based dispatch
struct maybe_refs_t {};
struct only_refs_t {};

constexpr maybe_refs_t maybe_refs {};
constexpr only_refs_t only_refs {};

template <class Fn, class... Args>
constexpr auto
adl_bind_front(bind_front_adl::maybe_refs_t, Fn&& fn, Args&&... args) {
  return bind_maybe_refs_front(
    std::forward<Fn>(fn), std::forward<Args>(args)...);
}

template <class Fn, class... Args>
constexpr auto
adl_bind_front(bind_front_adl::only_refs_t, Fn&& fn, Args&&... args) {
  return bind_refs_front(std::forward<Fn>(fn), std::forward<Args>(args)...);
}
}// namespace OpenKneeboard::weak_refs::bind_front_adl