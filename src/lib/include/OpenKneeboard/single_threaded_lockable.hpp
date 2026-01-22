// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
