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
#include <optional>
#include <stop_token>

#include <threadpoolapiset.h>
#include <winnt.h>

namespace OpenKneeboard::detail {
enum class ThreadPoolAwaitableState {
  Init,
  StartingWait,
  Waiting,
  HaveResult,// signaled, timeout, etc. *not* canceled
  Resuming,
  Resumed,
  Canceling,
  CancelingBeforeDispatch,
};
template <class Derived, class TResultType>
struct ThreadPoolAwaitable {
  using result_type = TResultType;

  ThreadPoolAwaitable(const std::stop_token stopToken)
    : mStopCallback(std::in_place, stopToken, this) {}
  ~ThreadPoolAwaitable() { mState.Assert(State::Resumed); }

  bool await_ready() const { return false; }

  auto await_resume() const {
    mState.Assert(State::Resumed);
    OPENKNEEBOARD_ASSERT(mResult != Derived::pending_result);
    return mResult;
  }

  void await_suspend(std::coroutine_handle<> coro) {
    OPENKNEEBOARD_ASSERT(!mCoro, "await_suspend called twice");
    mCoro = coro;

    if (const auto transitioned =
          mState.TryTransition<State::Init, State::StartingWait>();
        !transitioned) {
      OPENKNEEBOARD_ASSERT(
        transitioned.error() == State::CancelingBeforeDispatch);
      mResult = Derived::canceled_result;
      ResumeFrom<State::CancelingBeforeDispatch>();
      return;
    }

    const auto transitioned =
      mState.TryTransition<State::StartingWait, State::Waiting>();
    if (transitioned) {
      static_cast<Derived&>(*this).InitThreadPool();
      return;
    }

    OPENKNEEBOARD_ASSERT(transitioned.error() == State::Canceling);
    mResult = Derived::canceled_result;
    ResumeFrom<State::Canceling>();
  }

  template <result_type Result>
  void OnResult(PTP_CALLBACK_INSTANCE const pci) {
    const auto transition =
      mState.TryTransition<State::Waiting, State::HaveResult>();
    if (!transition) {
      OPENKNEEBOARD_ASSERT(transition.error() == State::Canceling);
      return;
    }

    DisassociateCurrentThreadFromCallback(pci);
    mResult = Result;
    ResumeFrom<State::HaveResult>();
  }

 private:
  using State = ThreadPoolAwaitableState;

  AtomicStateMachine<
    State,
    State::Init,
    std::array {
      // Normal path
      Transition {State::Init, State::StartingWait},
      Transition {State::StartingWait, State::Waiting},
      Transition {State::Waiting, State::HaveResult},
      Transition {State::HaveResult, State::Resuming},
      Transition {State::Resuming, State::Resumed},
      // Cancellation paths
      Transition {State::Init, State::CancelingBeforeDispatch},
      Transition {State::StartingWait, State::Canceling},
      Transition {State::Waiting, State::Canceling},
      Transition {State::CancelingBeforeDispatch, State::Resuming},
      Transition {State::Canceling, State::Resuming},
    }>
    mState;

  result_type mResult {Derived::pending_result};

  template <State From>
  void ResumeFrom() {
    OPENKNEEBOARD_ASSERT(mResult != Derived::pending_result);

    mState.Transition<From, State::Resuming>();
    mStopCallback.reset();

    const auto coro = std::exchange(mCoro, {});

    static_cast<Derived&>(*this).template CleanupThreadPool<From>();

    mState.Transition<State::Resuming, State::Resumed>();
    coro.resume();
  }

  void Cancel() {
    if (mState.TryTransition<State::Init, State::CancelingBeforeDispatch>()) {
      // handled by await_suspend
      return;
    }

    static_cast<Derived&>(*this).CancelThreadPool();
    if (mState.TryTransition<State::StartingWait, State::Canceling>()) {
      // handled by await_suspend
      return;
    }

    const auto transitioned =
      mState.TryTransition<State::Waiting, State::Canceling>();
    if (transitioned) {
      mResult = Derived::canceled_result;
      ResumeFrom<State::Canceling>();
      return;
    }

    using enum ThreadPoolAwaitableState;
    switch (transitioned.error()) {
      case Init:
      case StartingWait:
      case Waiting:
      case Canceling:
        fatal(
          "Impossible state {} in ThreadPoolAwaitable::cancel()",
          magic_enum::enum_name(transitioned.error()));
        break;
      case Resuming:
      case Resumed:
      case HaveResult:
        // nothing to do
        break;
    }
  }

 private:
  /* Slightly more efficient (especially space) than passing a capturing lambda
   * as std::function<void()>
   *
   * Standard specifically permits this to be reentrant into ~stop_callback,
   * which it is (via coro.resume());
   */
  struct Canceler {
    void operator()() const { mSelf->Cancel(); }
    Canceler() = delete;
    explicit Canceler(ThreadPoolAwaitable* self) : mSelf(self) {}

    ThreadPoolAwaitable* mSelf {nullptr};
  };

  std::coroutine_handle<> mCoro;
  std::optional<std::stop_callback<Canceler>> mStopCallback;
};

enum class SignalAwaitableResult {
  Pending,
  Success,
  Timeout,
  Canceled,
};
struct SignalAwaitable
  : ThreadPoolAwaitable<SignalAwaitable, SignalAwaitableResult> {
  static constexpr auto pending_result = SignalAwaitableResult::Pending;
  static constexpr auto canceled_result = SignalAwaitableResult::Canceled;

  template <class Rep, class Period = std::ratio<1>>
  SignalAwaitable(
    HANDLE const handle,
    const std::stop_token token,
    std::chrono::duration<Rep, Period> timeout)
    : ThreadPoolAwaitable<SignalAwaitable, SignalAwaitableResult>(token),
      mHandle(handle) {
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

  void InitThreadPool() {
    OPENKNEEBOARD_ASSERT(!mTPSignal);
    mTPSignal =
      CreateThreadpoolWait(&SignalAwaitable::ThreadPoolCallback, this, nullptr);
    SetThreadpoolWait(
      mTPSignal, mHandle, mTimeout ? &mTimeout.value() : nullptr);
  }

  template <ThreadPoolAwaitableState From>
  void CleanupThreadPool() {
    const auto wait = std::exchange(mTPSignal, nullptr);
    if (!wait) {
      return;
    }

    if constexpr (From == ThreadPoolAwaitableState::Canceling) {
      SetThreadpoolWait(wait, nullptr, nullptr);
      WaitForThreadpoolWaitCallbacks(wait, true);
    }
    CloseThreadpoolWait(wait);
  }

  void CancelThreadPool() {
    OPENKNEEBOARD_ASSERT(mTPSignal);
    SetThreadpoolWait(mTPSignal, nullptr, nullptr);
  }

 private:
  HANDLE mHandle {};
  PTP_WAIT mTPSignal {nullptr};
  std::optional<FILETIME> mTimeout {};

  static void ThreadPoolCallback(
    PTP_CALLBACK_INSTANCE const instance,
    void* context,
    PTP_WAIT,
    const TP_WAIT_RESULT result) {
    auto& self = *reinterpret_cast<SignalAwaitable*>(context);
    if (result == WAIT_OBJECT_0) {
      self.OnResult<SignalAwaitableResult::Success>(instance);
      return;
    }
    OPENKNEEBOARD_ASSERT(result == WAIT_TIMEOUT);
    self.OnResult<SignalAwaitableResult::Timeout>(instance);
  }
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

enum class ResumeOnSignalError {
  Timeout,
  Canceled,
};

/** Alternative to `winrt::resume_on_signal()` that can cancel without
 * exceptions.
 *
 * The primary benefit is that this makes breaking on all exceptions practical
 * when debugging program shutdown; as well as the irrelevant breakpoints,
 * it can also effect timing, making intermittent issues harder to reproduce.
 */
template <class T>
task<std::expected<void, ResumeOnSignalError>>
resume_on_signal(HANDLE const handle, std::stop_token token, T&& timeout) {
  const auto result = co_await detail::SignalAwaitable {
    handle,
    token,
    std::forward<T>(timeout),
  };
  using enum detail::SignalAwaitableResult;
  switch (result) {
    case Pending:
      fatal("SignalAwaitable returned 'pending'");
    case Success:
      co_return {};
    case Timeout:
      co_return std::unexpected {ResumeOnSignalError::Timeout};
    case Canceled:
      co_return std::unexpected {ResumeOnSignalError::Canceled};
  }
  __assume(false);
}

inline auto resume_on_signal(HANDLE const handle, const std::stop_token token) {
  return resume_on_signal(handle, token, std::chrono::milliseconds::zero());
}

inline task<void> resume_on_signal(HANDLE const handle) {
  std::ignore =
    co_await resume_on_signal(handle, {}, std::chrono::milliseconds::zero());
  co_return;
}

}// namespace OpenKneeboard
