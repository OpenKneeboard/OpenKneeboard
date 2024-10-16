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
#include <functional>
#include <optional>
#include <stop_token>

#include <threadpoolapiset.h>
#include <winnt.h>

namespace OpenKneeboard {
enum class TimerResult {
  Timeout,
  Cancelled,
};
}

namespace OpenKneeboard::detail {
struct TimerAwaitable {
  template <class Rep, class Period = std::ratio<1>>
  TimerAwaitable(
    std::chrono::duration<Rep, Period> delay,
    std::stop_token stopToken)
    : mStopCallback(stopToken, std::bind_front(&TimerAwaitable::cancel, this)) {
    OPENKNEEBOARD_ASSERT(
      delay > decltype(delay)::zero(), "Sleep duration must be > 0");

    // negative filetime = interval, positive = absolute
    const auto count = static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::file_clock::duration>(delay)
        .count());
    ULARGE_INTEGER ulDuration {.QuadPart = std::bit_cast<uint64_t>(-count)};
    mTime = {
      .dwLowDateTime = ulDuration.LowPart,
      .dwHighDateTime = ulDuration.HighPart,
    };
  }

  inline TimerResult result() const {
    OPENKNEEBOARD_ASSERT(mResult.has_value());
    return mResult.value();
  }

  inline ~TimerAwaitable() {
    OPENKNEEBOARD_ASSERT(mTPTimer);
    OPENKNEEBOARD_ASSERT(mResult.has_value());
    CloseThreadpoolTimer(mTPTimer);
  }

  inline bool await_ready() const {
    return false;
  }

  inline void await_resume() const {
  }

  inline void await_suspend(std::coroutine_handle<> coro) {
    mCoro = std::move(coro);
    mTPTimer = CreateThreadpoolTimer(
      [](auto, auto rawThis, auto) {
        auto self = reinterpret_cast<TimerAwaitable*>(rawThis);
        self->mResult = TimerResult::Timeout;
        self->mCoro.resume();
        self->mCoro = {};
      },
      this,
      nullptr);
    SetThreadpoolTimer(mTPTimer, &mTime, 0, 0);
  }

 private:
  std::stop_callback<std::function<void()>> mStopCallback;

  inline void cancel() {
    OPENKNEEBOARD_ASSERT(mTPTimer);

    if (SetThreadpoolTimerEx(mTPTimer, nullptr, 0, 0)) {
      mResult = TimerResult::Cancelled;
    } else {
      // Dispatched, but given we're being cancelled, presumably in progress
      mResult = TimerResult::Timeout;
    }
  }

  FILETIME mTime {};
  PTP_TIMER mTPTimer {nullptr};
  std::coroutine_handle<> mCoro;
  std::optional<TimerResult> mResult;
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

/** Alternative to `winrt::resume_after()` that can cancel without exceptions.
 *
 * The primary benefit is that this makes breaking on all exceptions practical
 * when debugging program shutdown; as well as the irrelevant breakpoints,
 * it can also effect timing, making intermittent issues harder to reproduce.
 */
inline task<TimerResult> resume_after(auto duration, std::stop_token token) {
  detail::TimerAwaitable impl(duration, token);
  co_await impl;
  co_return impl.result();
}

}// namespace OpenKneeboard