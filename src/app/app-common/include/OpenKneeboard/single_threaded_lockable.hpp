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
#include <OpenKneeboard/dprint.hpp>

#include <system_error>
#include <utility>

namespace OpenKneeboard {

/** A class implementing the `lockable` C++ named requirements, but with no
 * thread safety.
 *
 * This lets you use `std::unique_lock()` etc without the overhead of
 * traditional atomics, e.g. to detect unwanted recursion.
 */
class single_threaded_lockable final {
 public:
  constexpr single_threaded_lockable() = default;
  inline ~single_threaded_lockable() {
    if (mLocked) [[unlikely]] {
      fatal("In ~single_threaded_lockable() while locked");
    }
  }

  bool try_lock() {
    const auto previouslyLocked = std::exchange(mLocked, true);
    return !previouslyLocked;
  }

  void lock() {
    if (!try_lock()) {
      throw std::system_error {
        std::make_error_code(std::errc::resource_deadlock_would_occur)};
    }
  }

  void unlock() {
    if (!std::exchange(mLocked, false)) [[unlikely]] {
      throw std::logic_error("Attempting to unlock, but not locked");
    }
  }

 private:
  bool mLocked {false};
};

}// namespace OpenKneeboard