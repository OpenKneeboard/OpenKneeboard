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

#include <OpenKNeeboard/format/enum.hpp>

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
    const auto state = mState.Get();
    switch (state) {
      case State::Signalled:
        return {};
      case State::Timeout:
        return std::unexpected {ResumeOnSignalError::Timeout};
      case State::Cancelled:
      case State::CancelledBeforeWait:
        return std::unexpected {ResumeOnSignalError::Cancelled};
      default:
        fatal("Can't fetch result for a SignalAwaitable with state {}", state);
    }
  }

  inline ~SignalAwaitable() {
    if (mTPSignal) {
      CloseThreadpoolWait(mTPSignal);
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
    mTPSignal = CreateThreadpoolWait(
      [](auto, auto rawThis, auto, const TP_WAIT_RESULT result) {
        auto self = reinterpret_cast<SignalAwaitable*>(rawThis);
        if (result == WAIT_OBJECT_0) {
          self->mState.Transition<State::Waiting, State::Signalled>();
        } else {
          OPENKNEEBOARD_ASSERT(result == WAIT_TIMEOUT);
          self->mState.Transition<State::Waiting, State::Timeout>();
        }
        self->mCoro.resume();
      },
      this,
      nullptr);
    mState.Transition<State::StartingWait, State::Waiting>();
    SetThreadpoolWait(
      mTPSignal, mHandle, mTimeout ? &mTimeout.value() : nullptr);
  }

 private:
  enum class State {
    Init,
    CancelledBeforeWait,
    StartingWait,
    Waiting,
    Signalled,
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
      Transition {State::Waiting, State::Signalled},
      Transition {State::Waiting, State::Timeout},
      Transition {State::Waiting, State::Cancelled},
    }>
    mState;

  inline void cancel() {
    if (mState.TryTransition<State::Init, State::CancelledBeforeWait>()) {
      return;
    }
    OPENKNEEBOARD_ASSERT(mTPSignal);

    if (SetThreadpoolWaitEx(mTPSignal, NULL, nullptr, 0)) {
      mState.Transition<State::Waiting, State::Cancelled>();
      mCoro.resume();
    }
  }

  HANDLE mHandle {};
  std::optional<FILETIME> mTimeout {};
  PTP_WAIT mTPSignal {nullptr};
  std::coroutine_handle<> mCoro;

  std::stop_callback<std::function<void()>> mStopCallback;
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