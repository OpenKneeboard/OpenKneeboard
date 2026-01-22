// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/format/enum.hpp>
#include <OpenKneeboard/task.hpp>

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

  TimerResult result() const {
    switch (const auto state = mState.Get()) {
      case State::Timeout:
        return TimerResult::Timeout;
      case State::Cancelled:
      case State::CancelledBeforeWait:
        return TimerResult::Cancelled;
      default:
        fatal("Can't get result from TimerAwaitable in state {}", state);
    }
  }

  ~TimerAwaitable() {
    mStopCallback.reset();

    using enum State;
    switch (const auto state = mState.Get())
    {
    case Init:
    case StartingWait:
    case Waiting:
      fatal("resume_after torn down in invalid state {}", magic_enum::enum_name(state));
    case CancelledBeforeWait:
      OPENKNEEBOARD_ASSERT(!mTPTimer, "Cancelled before wait, but have a wait");
      break;
    case Cancelled:
      OPENKNEEBOARD_ASSERT(mTPTimer, "cancelled after wait, but no wait");
      SetThreadpoolTimer(mTPTimer, nullptr, 0, 0);
      WaitForThreadpoolTimerCallbacks(mTPTimer, true);
      [[fallthrough]];
    case Timeout:
      OPENKNEEBOARD_ASSERT(mTPTimer, "timeout or signalled, but no wait");
      CloseThreadpoolTimer(mTPTimer);
      break;

    }
  }

  bool await_ready() const {
    return false;
  }

  void await_resume() const {
  }

  void await_suspend(std::coroutine_handle<> coro) {
    if (!mState.TryTransition<State::Init, State::StartingWait>()) {
      mState.Assert(State::CancelledBeforeWait);
      coro.resume();
      return;
    }

    mCoro = std::move(coro);
    mTPTimer = CreateThreadpoolTimer(
      [](auto, auto rawThis, auto) {
        auto self = reinterpret_cast<TimerAwaitable*>(rawThis);
        const auto transition = self->mState.TryTransition<State::Waiting, State::Timeout>();
        if (transition) {
          self->mCoro.resume();
          return;
        }
        OPENKNEEBOARD_ASSERT(transition.error() == State::Cancelled);
      },
      this,
      nullptr);
    const auto transitioned = mState.TryTransition<State::StartingWait, State::Waiting>();
    if (transitioned)
    {
      SetThreadpoolTimer(mTPTimer, &mTime, 0, 0);
      return;
    }
    OPENKNEEBOARD_ASSERT(transitioned.error() == State::Cancelled);
    coro.resume();
    // Cleanup is dealt with in ~TimerAwaitable
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
      Transition {State::StartingWait, State::Cancelled},
      Transition {State::Waiting, State::Timeout},
      Transition {State::Waiting, State::Cancelled},
    }>
    mState;

  void cancel()
  {
    if (mState.TryTransition<State::Init, State::CancelledBeforeWait>()) {
      return;
    }

    OPENKNEEBOARD_ASSERT(mTPTimer);

    SetThreadpoolTimerEx(mTPTimer, nullptr, 0, 0);
    if (mState.TryTransition<State::StartingWait, State::Cancelled>()) {
      // handled by await_suspend
      return;
    }

    const auto transitioned = mState.TryTransition<State::Waiting, State::Cancelled>();
    if (transitioned)
    {
      mCoro.resume();
      return;
    }

    using enum State;
    switch (transitioned.error()) {
      case Init:
      case CancelledBeforeWait:
      case StartingWait:
      case Waiting:
      case Cancelled:
        fatal("Impossible state {} in TimerAwaitable::cancel()", magic_enum::enum_name(transitioned.error()));
        break;
      case Timeout:
        // nothing to do
        break;
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
task<TimerResult> resume_after(auto duration, std::stop_token token = {}) {
  detail::TimerAwaitable impl(duration, token);
  co_await impl;
  co_return impl.result();
}

}// namespace OpenKneeboard