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
  Canceled,
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
    mState.Assert(State::Resumed);
    using enum Result;
    switch (mResult) {
      case Success:
        return {};
      case Timeout:
        return std::unexpected {ResumeOnSignalError::Timeout};
      case Canceled:
        return std::unexpected {ResumeOnSignalError::Canceled};
      case Pending:
        fatal("resume_on_signal::result() called while pending");
    }
    __assume(false);
  }

  ~SignalAwaitable() { mState.Assert(State::Resumed); }

  bool await_ready() const { return false; }

  void await_resume() const {}
  void await_suspend(std::coroutine_handle<> coro) {
    OPENKNEEBOARD_ASSERT(!mCoro, "await_suspend called twice");
    mCoro = coro;

    if (!mState.TryTransition<State::Init, State::StartingWait>()) {
      mResult = Result::Canceled;
      resume_from<State::CanceledBeforeWait>();
      return;
    }

    mTPSignal = CreateThreadpoolWait(
      [](const auto ctx, auto rawThis, auto, const TP_WAIT_RESULT result) {
        auto& self = *reinterpret_cast<SignalAwaitable*>(rawThis);
        if (result == WAIT_OBJECT_0) {
          self.thread_pool_callback<State::Signaled, Result::Success>(ctx);
          return;
        }
        OPENKNEEBOARD_ASSERT(result == WAIT_TIMEOUT);
        self.thread_pool_callback<State::Timeout, Result::Timeout>(ctx);
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
    // Specifically, StartingWait -> Canceled is our responsibility
    OPENKNEEBOARD_ASSERT(transitioned.error() == State::Canceled);
    mResult = Result::Canceled;
    resume_from<State::Canceled>();
  }

 private:
  enum class State {
    Init,
    CanceledBeforeWait,
    StartingWait,
    Waiting,
    Signaled,
    Timeout,
    Canceled,
    Resuming,
    Resumed,
  };
  AtomicStateMachine<
    State,
    State::Init,
    std::array {
      Transition {State::Init, State::CanceledBeforeWait},
      Transition {State::Init, State::StartingWait},
      Transition {State::StartingWait, State::Canceled},
      Transition {State::StartingWait, State::Waiting},
      Transition {State::Waiting, State::Signaled},
      Transition {State::Waiting, State::Timeout},
      Transition {State::Waiting, State::Canceled},
      // Resumed from any canceled or completed state
      Transition {State::Signaled, State::Resuming},
      Transition {State::Timeout, State::Resuming},
      Transition {State::Canceled, State::Resuming},
      Transition {State::CanceledBeforeWait, State::Resuming},
      Transition {State::Resuming, State::Resumed},
    }>
    mState;

  enum class Result {
    Pending,
    Success,
    Timeout,
    Canceled,
  };
  Result mResult {Result::Pending};

  template <State From>
  void resume_from() {
    OPENKNEEBOARD_ASSERT(mResult != Result::Pending);

    mState.Transition<From, State::Resuming>();
    mStopCallback.reset();

    const auto wait = std::exchange(mTPSignal, nullptr);
    const auto coro = std::exchange(mCoro, {});

    if constexpr (
      From == State::Canceled || From == State::CanceledBeforeWait) {
      if (wait) {
        SetThreadpoolWait(wait, nullptr, nullptr);
        WaitForThreadpoolWaitCallbacks(wait, true);
      }
    }
    if (wait) {
      CloseThreadpoolWait(wait);
    }

    mState.Transition<State::Resuming, State::Resumed>();
    coro.resume();
  }

  void cancel() {
    if (mState.TryTransition<State::Init, State::CanceledBeforeWait>()) {
      return;
    }
    OPENKNEEBOARD_ASSERT(mTPSignal);

    SetThreadpoolWaitEx(mTPSignal, NULL, nullptr, 0);
    if (mState.TryTransition<State::StartingWait, State::Canceled>()) {
      // handled by await_suspend
      return;
    }

    const auto transitioned =
      mState.TryTransition<State::Waiting, State::Canceled>();
    if (transitioned) {
      mResult = Result::Canceled;
      resume_from<State::Canceled>();
      return;
    }
    using enum State;
    switch (transitioned.error()) {
      case Init:
      case CanceledBeforeWait:
      case StartingWait:
      case Waiting:
      case Canceled:
        fatal(
          "Impossible state {} in SignalAwaitable::cancel()",
          magic_enum::enum_name(transitioned.error()));
        break;
      case Resuming:
      case Resumed:
      case Signaled:
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

  template <State TTargetState, Result TResult>
  void thread_pool_callback(PTP_CALLBACK_INSTANCE const pci) {
    const auto transition =
      mState.TryTransition<State::Waiting, TTargetState>();
    if (!transition) {
      OPENKNEEBOARD_ASSERT(transition.error() == State::Canceled);
      return;
    }

    DisassociateCurrentThreadFromCallback(pci);
    mResult = TResult;
    resume_from<TTargetState>();
  }
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
