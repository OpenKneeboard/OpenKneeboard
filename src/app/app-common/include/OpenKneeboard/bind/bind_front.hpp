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

#include "detail/static_const.hpp"

namespace OpenKneeboard::bind_detail {

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

}// namespace OpenKneeboard::bind_detail

namespace OpenKneeboard::bind::inline bind_core {

namespace {
constexpr auto const& bind_front
  = bind_detail::static_const<bind_detail::bind_front_fn>;
}

}// namespace OpenKneeboard::bind