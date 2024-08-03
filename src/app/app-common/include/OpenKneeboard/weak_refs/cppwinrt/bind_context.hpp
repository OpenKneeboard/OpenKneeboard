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

#include <wil/cppwinrt_helpers.h>

#include <OpenKneeboard/weak_refs/bind_front_adl.hpp>

#include <concepts>
#include <functional>

namespace OpenKneeboard::weak_refs::cppwinrt {

template <class Context, class F>
auto bind_context(Context&& context, F&& f) {
  return std::bind_front(
    []<class... Args>(auto context, auto f, Args&&... args)

      -> winrt::fire_and_forget {
      if constexpr (std::same_as<
                      std::decay_t<Context>,
                      winrt::apartment_context>) {
        co_await context;
      } else {
        co_await wil::resume_foreground(context);
      }
      std::invoke(f, std::forward<Args>(args)...);
    },
    std::forward<Context>(context),
    std::forward<F>(f));
}

template <class Context, class... Args>
auto bind_front_with_context(Context&& context, Args&&... args) {
  using namespace weak_refs::bind_front_adl;
  return bind_context(
    std::forward<Context>(context), bind_front(std::forward<Args>(args)...));
}

}// namespace OpenKneeboard::weak_refs::cppwinrt