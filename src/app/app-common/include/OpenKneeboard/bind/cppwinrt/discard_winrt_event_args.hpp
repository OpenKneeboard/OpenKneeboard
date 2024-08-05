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

#include "../../cppwinrt/concepts.hpp"

namespace OpenKneeboard::bind_detail::cppwinrt {
using namespace ::OpenKneeboard::cppwinrt::cppwinrt_concepts;

template <class... Args>
struct bind_trace_t {};

struct discard_winrt_event_args_t {};

template <class TNext, class BindTrace>
struct discard_winrt_event_args_fn {
  TNext mNext;

  template <cppwinrt_type TSender, cppwinrt_type TEventArgs, class... Extras>
    requires std::invocable<TNext, Extras...>
  constexpr auto
  operator()(Extras&&... extras, const TSender&, const TEventArgs&) const {
    return std::invoke(mNext, std::forward<Extras>(extras)...);
  }
};

template <class Fn, class... Args>
constexpr auto
adl_bind_front(discard_winrt_event_args_t, Fn&& fn, Args&&... args) {
  // Fully-qualify so that ADL doesn't lead us to `std::bind_front()` if `fn` is
  // an `std::function<>`
  const auto next = ::OpenKneeboard::bind::bind_front(
    std::forward<Fn>(fn), std::forward<Args>(args)...);

  return discard_winrt_event_args_fn<
    std::decay_t<decltype(next)>,
    bind_trace_t<Args...>> {next};
}
}// namespace OpenKneeboard::bind_detail::cppwinrt

namespace OpenKneeboard::bind::inline cppwinrt_bind_tags {
constexpr bind_detail::cppwinrt::discard_winrt_event_args_t
  discard_winrt_event_args {};

}