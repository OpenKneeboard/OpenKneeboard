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

#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/format/enum.hpp>
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
  TimerAwaitable() = delete;
  TimerAwaitable(const TimerAwaitable&) = delete;
  TimerAwaitable(TimerAwaitable&&) = delete;
  TimerAwaitable& operator=(const TimerAwaitable&) = delete;
  TimerAwaitable& operator=(TimerAwaitable&&) = delete;

  template <class Rep, class Period = std::ratio<1>>
  TimerAwaitable(
    std::chrono::duration<Rep, Period> delay,
    std::stop_token stopToken)
    : mStopCallback(
        std::in_place,
        stopToken,
        std::bind_front(&TimerAwaitable::cancel, this)) {
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
    const auto state = mState.Get();
    switch (state) {
      case State::Timeout:
        return TimerResult::Timeout;
      case State::Cancelled:
      case State::CancelledBeforeWait:
        return TimerResult::Cancelled;
      default:
        fatal("Can't get result from TimerAwaitable in state {}", state);
    }
  }

  inline ~TimerAwaitable() {
    mStopCallback.reset();

    if (mTPTimer) {
      CloseThreadpoolTimer(mTPTimer);
    }

    const auto state = mState.Get();
    OPENKNEEBOARD_ASSERT(
      state != State::Init && state != State::StartingWait
      && state != State::Waiting);
  }

  inline bool await_ready() const {
    return false;
  }

  inline void await_resume() const {
  }

  inline void await_suspend(std::coroutine_handle<> coro) {
    if (!mState.TryTransition<State::Init, State::StartingWait>()) {
      mState.Assert(State::CancelledBeforeWait);
      coro.resume();
      return;
    }

    mCoro = std::move(coro);
    mTPTimer = CreateThreadpoolTimer(
      [](auto, auto rawThis, auto) {
        auto self = reinterpret_cast<TimerAwaitable*>(rawThis);
        self->mState.Transition<State::Waiting, State::Timeout>();
        self->mCoro.resume();
      },
      this,
      nullptr);
    mState.Transition<State::StartingWait, State::Waiting>();
    SetThreadpoolTimer(mTPTimer, &mTime, 0, 0);
  }

 private:
  enum class State {
    Init,
    CancelledBeforeWait,
    StartingWait,
    Waiting,
    Timeout,
    Cancelled,
  };
  AtomicStateMachine<
    State,
    State::Init,
    std::array {
      Transition {State::Init, State::CancelledBeforeWait},
      Transition {State::Init, State::StartingWait},
      Transition {State::StartingWait, State::Waiting},
      Transition {State::Waiting, State::Timeout},
      Transition {State::Waiting, State::Cancelled},
    }>
    mState;

  inline void cancel() {
    if (mState.TryTransition<State::Init, State::CancelledBeforeWait>()) {
      return;
    }

    OPENKNEEBOARD_ASSERT(mTPTimer);

    if (SetThreadpoolTimerEx(mTPTimer, nullptr, 0, 0)) {
      mState.Transition<State::Waiting, State::Cancelled>();
      mCoro.resume();
    }
  }

  FILETIME mTime {};
  PTP_TIMER mTPTimer {nullptr};
  std::coroutine_handle<> mCoro;

  std::optional<std::stop_callback<std::function<void()>>> mStopCallback;
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