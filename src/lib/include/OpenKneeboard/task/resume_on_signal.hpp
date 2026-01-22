// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/format/enum.hpp>
#include <OpenKneeboard/task.hpp>

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
        std::in_place,
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

  ResumeOnSignalResult result() const {
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

  ~SignalAwaitable() {
    // We definitely don't want to invoke the callback while we're in the
    // destructor
    mStopCallback.reset();

    using enum State;
    switch (const auto state = mState.Get()) {
      case Init:
      case StartingWait:
      case Waiting:
        fatal(
          "resume_on_signal torn down in invalid state {}",
          magic_enum::enum_name(state));
      case CancelledBeforeWait:
        OPENKNEEBOARD_ASSERT(
          !mTPSignal, "Cancelled before wait, but have a wait");
        break;
      case Cancelled:
        OPENKNEEBOARD_ASSERT(mTPSignal, "cancelled after wait, but no wait");
        SetThreadpoolWait(mTPSignal, nullptr, nullptr);
        WaitForThreadpoolWaitCallbacks(mTPSignal, true);
        [[fallthrough]];
      case Timeout:
      case Signalled:
        OPENKNEEBOARD_ASSERT(mTPSignal, "timeout or signalled, but no wait");
        CloseThreadpoolWait(mTPSignal);
        break;
    }
  }

  bool await_ready() const { return false; }

  void await_resume() const {}

  void await_suspend(std::coroutine_handle<> coro) {
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
          const auto transition =
            self->mState.TryTransition<State::Waiting, State::Signalled>();
          if (transition) {
            self->mCoro.resume();
            return;
          }
          OPENKNEEBOARD_ASSERT(transition.error() == State::Cancelled);
        } else {
          OPENKNEEBOARD_ASSERT(result == WAIT_TIMEOUT);
          const auto transition =
            self->mState.TryTransition<State::Waiting, State::Timeout>();
          if (transition) {
            self->mCoro.resume();
            return;
          }
          OPENKNEEBOARD_ASSERT(transition.error() == State::Cancelled);
        }
      },
      this,
      nullptr);
    const auto transitioned =
      mState.TryTransition<State::StartingWait, State::Waiting>();
    if (transitioned) {
      SetThreadpoolWait(
        mTPSignal, mHandle, mTimeout ? &mTimeout.value() : nullptr);
      return;
    }
    OPENKNEEBOARD_ASSERT(transitioned.error() == State::Cancelled);
    coro.resume();
    // Cleanup of the wait is dealt with in ~SignalAwaitable();
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
      Transition {State::StartingWait, State::Cancelled},
      Transition {State::StartingWait, State::Waiting},
      Transition {State::Waiting, State::Signalled},
      Transition {State::Waiting, State::Timeout},
      Transition {State::Waiting, State::Cancelled},
    }>
    mState;

  void cancel() {
    if (mState.TryTransition<State::Init, State::CancelledBeforeWait>()) {
      return;
    }
    OPENKNEEBOARD_ASSERT(mTPSignal);

    SetThreadpoolWaitEx(mTPSignal, NULL, nullptr, 0);
    if (mState.TryTransition<State::StartingWait, State::Cancelled>()) {
      // handled by await_suspend
      return;
    }

    const auto transitioned =
      mState.TryTransition<State::Waiting, State::Cancelled>();
    if (transitioned) {
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
        fatal(
          "Impossible state {} in SignalAwaitable::cancel()",
          magic_enum::enum_name(transitioned.error()));
        break;
      case Signalled:
      case Timeout:
        // nothing to do
        break;
    }
  }

  HANDLE mHandle {};
  std::optional<FILETIME> mTimeout {};
  PTP_WAIT mTPSignal {nullptr};
  std::coroutine_handle<> mCoro;

  std::optional<std::stop_callback<std::function<void()>>> mStopCallback;
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
template <class T>
task<ResumeOnSignalResult>
resume_on_signal(HANDLE handle, std::stop_token token, T&& timeout) {
  detail::SignalAwaitable impl(handle, token, std::forward<T>(timeout));
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
