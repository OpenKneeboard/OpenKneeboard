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
#include <functional>

namespace OpenKneeboard {

// Roughly equivalent to `std::experimental::scope_exit`, but that isn't wideley
// available yet
template <std::invocable<> TCallback>
class scope_exit final {
 private:
  std::decay_t<TCallback> mCallback;

 public:
  constexpr scope_exit(TCallback&& f) : mCallback(std::forward<TCallback>(f)) {
  }

  ~scope_exit() noexcept {
    std::invoke(mCallback);
  }

  scope_exit() = delete;
  scope_exit(const scope_exit&) = delete;
  scope_exit(scope_exit&&) noexcept = delete;
  scope_exit& operator=(const scope_exit&) = delete;
  scope_exit& operator=(scope_exit&&) noexcept = delete;
};

}// namespace OpenKneeboard
