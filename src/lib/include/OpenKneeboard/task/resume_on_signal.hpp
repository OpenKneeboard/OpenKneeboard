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

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <chrono>
#include <coroutine>
#include <expected>
#include <functional>
#include <optional>
#include <stop_token>

#include <threadpoolapiset.h>
#include <winnt.h>

namespace OpenKneeboard {

enum class ResumeOnSignalError {
  Timeout,
  Cancelled,
};
using ResumeOnSignalResult = std::expected<void, ResumeOnSignalError>;

}// namespace OpenKneeboard

namespace OpenKneeboard::detail {
struct SignalAwaitable {
  template <class Rep, class Period = std::ratio<1>>
  SignalAwaitable(
    HANDLE handle,
    std::stop_token stopToken,
    std::chrono::duration<Rep, Period> timeout)
    : mHandle(handle),
      mStopCallback(
        stopToken,
        std::bind_front(&SignalAwaitable::cancel, this)) {
    if (timeout == decltype(timeout)::zero()) {
      return;
    }
    OPENKNEEBOARD_ASSERT(
      timeout > decltype(timeout)::zero(), "Sleep duration must be >= 0");

    // negative filetime = interval, positive = absolute
    const auto count = static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::file_clock::duration>(timeout)
        .count());
    ULARGE_INTEGER ulDuration {.QuadPart = std::bit_cast<uint64_t>(-count)};
    mTimeout = FILETIME {
      .dwLowDateTime = ulDuration.LowPart,
      .dwHighDateTime = ulDuration.HighPart,
    };
  }

  inline ResumeOnSignalResult result() const {
    OPENKNEEBOARD_ASSERT(mResult.has_value());
    return mResult.value();
  }

  inline ~SignalAwaitable() {
    OPENKNEEBOARD_ASSERT(mTPSignal);
    OPENKNEEBOARD_ASSERT(mResult.has_value());
    CloseThreadpoolWait(mTPSignal);
  }

  inline bool await_ready() const {
    return false;
  }

  inline void await_resume() const {
  }

  inline void await_suspend(std::coroutine_handle<> coro) {
    mCoro = std::move(coro);
    mTPSignal = CreateThreadpoolWait(
      [](auto, auto rawThis, auto, const TP_WAIT_RESULT result) {
        auto self = reinterpret_cast<SignalAwaitable*>(rawThis);
        if (result == WAIT_OBJECT_0) {
          self->mResult = ResumeOnSignalResult {};
        } else {
          OPENKNEEBOARD_ASSERT(result == WAIT_TIMEOUT);
          self->mResult = std::unexpected {ResumeOnSignalError::Timeout};
        }
        self->mCoro.resume();
        self->mCoro = {};
      },
      this,
      nullptr);
    SetThreadpoolWait(
      mTPSignal, mHandle, mTimeout ? &mTimeout.value() : nullptr);
  }

 private:
  std::stop_callback<std::function<void()>> mStopCallback;

  inline void cancel() {
    OPENKNEEBOARD_ASSERT(mTPSignal);

    if (SetThreadpoolWaitEx(mTPSignal, NULL, nullptr, 0)) {
      mResult = std::unexpected {ResumeOnSignalError::Cancelled};
      mCoro.resume();
      mCoro = {};
    }
  }

  HANDLE mHandle {};
  std::optional<FILETIME> mTimeout {};
  PTP_WAIT mTPSignal {nullptr};
  std::coroutine_handle<> mCoro;
  std::optional<ResumeOnSignalResult> mResult;
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

/** Alternative to `winrt::resume_on_signal()` that can cancel without
 * exceptions.
 *
 * The primary benefit is that this makes breaking on all exceptions practical
 * when debugging program shutdown; as well as the irrelevant breakpoints,
 * it can also effect timing, making intermittent issues harder to reproduce.
 */
inline task<ResumeOnSignalResult>
resume_on_signal(HANDLE handle, std::stop_token token, auto timeout) {
  detail::SignalAwaitable impl(handle, token, timeout);
  co_await impl;
  co_return impl.result();
}

/** Alternative to `winrt::resume_on_signal()` that can cancel without
 * exceptions.
 *
 * The primary benefit is that this makes breaking on all exceptions practical
 * when debugging program shutdown; as well as the irrelevant breakpoints,
 * it can also effect timing, making intermittent issues harder to reproduce.
 */
inline task<ResumeOnSignalResult> resume_on_signal(
  HANDLE handle,
  std::stop_token token) {
  detail::SignalAwaitable impl(
    handle, token, std::chrono::milliseconds::zero());
  co_await impl;
  co_return impl.result();
}

}// namespace OpenKneeboard