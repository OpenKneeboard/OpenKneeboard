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

#include <concepts>
#include <exception>
#include <functional>

namespace OpenKneeboard::detail {

enum class basic_scope_execution_policy {
  Always,
  OnFailure,
  OnSuccess,
};

// Roughly equivalent to `std::experimental::scope_exit`, but that isn't wideley
// available yet
template <basic_scope_execution_policy TWhen, std::invocable<> TCallback>
class basic_scope_exit final {
 private:
  std::decay_t<TCallback> mCallback;
  int mInitialUncaught = std::uncaught_exceptions();

  using enum basic_scope_execution_policy;

 public:
  constexpr basic_scope_exit(TCallback&& f)
    : mCallback(std::forward<TCallback>(f)) {
  }

  ~basic_scope_exit() noexcept
    requires(TWhen == Always)
  {
    std::invoke(mCallback);
  }

  ~basic_scope_exit() noexcept
    requires(TWhen == OnFailure)
  {
    if (std::uncaught_exceptions() > mInitialUncaught) {
      auto it = std::current_exception();
      std::invoke(mCallback);
    }
  }
  ~basic_scope_exit() noexcept
    requires(TWhen == OnSuccess)
  {
    if (std::uncaught_exceptions() == mInitialUncaught) {
      std::invoke(mCallback);
    }
  }

  basic_scope_exit() = delete;
  basic_scope_exit(const basic_scope_exit&) = delete;
  basic_scope_exit(basic_scope_exit&&) noexcept = delete;
  basic_scope_exit& operator=(const basic_scope_exit&) = delete;
  basic_scope_exit& operator=(basic_scope_exit&&) noexcept = delete;
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

template <class T>
using scope_exit
  = detail::basic_scope_exit<detail::basic_scope_execution_policy::Always, T>;
template <class T>
using scope_fail = detail::
  basic_scope_exit<detail::basic_scope_execution_policy::OnFailure, T>;
template <class T>
using scope_success = detail::
  basic_scope_exit<detail::basic_scope_execution_policy::OnSuccess, T>;
}// namespace OpenKneeboard