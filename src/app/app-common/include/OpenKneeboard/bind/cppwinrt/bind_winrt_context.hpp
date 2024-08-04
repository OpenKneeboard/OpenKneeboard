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

#include "../../cppwinrt/bind_context.hpp"
#include "../bind_front.hpp"

namespace OpenKneeboard::bind_detail {

struct bind_winrt_context_t {};

template <class Context, class Fn, class... Args>
constexpr auto adl_bind_front(
  bind_winrt_context_t,
  Fn&& fn,
  Context&& context,
  Args&&... args) {
  using namespace OpenKneeboard::bind;
  // Fully-qualify so that ADL doesn't lead us to `std::bind_front()` if `fn` is
  // an `std::function<>`
  const auto next = ::OpenKneeboard::bind::bind_front(
    std::forward<Fn>(fn), std::forward<Args>(args)...);

  return OpenKneeboard::cppwinrt::bind_context(
    std::forward<Context>(context), next);
}

}// namespace OpenKneeboard::bind_detail

namespace OpenKneeboard::bind::inline cppwinrt_bind_tags {
// Marker for ADL
constexpr bind_detail::bind_winrt_context_t bind_winrt_context;
}// namespace OpenKneeboard::bind::inline cppwinrt_bind_tags