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

#include <coroutine>
#include <expected>
#include <optional>
#include <stop_token>

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

    static_cast<Derived&>(*this).InitThreadPool();

    const auto transitioned =
      mState.TryTransition<State::StartingWait, State::Waiting>();
    if (transitioned) {
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
      OPENKNEEBOARD_ASSERT(
        transition.error() == State::Canceling,
        "unexpected state: {}",
        transition.error());
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
      case CancelingBeforeDispatch:
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

}// namespace OpenKneeboard::detail
