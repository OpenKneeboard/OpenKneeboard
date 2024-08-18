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

#include <shims/winrt/base.h>

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <functional>
#include <type_traits>

namespace OpenKneeboard::detail {
template <class T>
concept awaitable = requires(T v) { static_cast<T&&>(v).operator co_await(); };
}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

template <class TDQ, class TFn>
auto TryEnqueue(TDQ dq, TFn&& fn) {
  dq.TryEnqueue(std::bind_front(
    [](auto fn) -> OpenKneeboard::fire_and_forget {
      try {
        if constexpr (detail::awaitable<std::invoke_result_t<TFn>>) {
          co_await std::invoke(fn);
        } else {
          std::invoke(fn);
          co_return;
        }
      } catch (...) {
        fatal_with_exception(std::current_exception());
      }
    },
    std::forward<TFn>(fn)));
}

}// namespace OpenKneeboard