// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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