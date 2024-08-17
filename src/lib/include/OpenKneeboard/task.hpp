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

#include <shims/winrt/base.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>

#include <atomic>
#include <coroutine>
#include <memory>

namespace OpenKneeboard::detail {

enum class TaskPromiseState : uintptr_t {
  Running = 1,
  Abandoned = 2,
  Completed = 3,
};

/* Union so we can do std::atomic.
 *
 * - TaskPromiseState is sizeof(ptr), but always an invalid pointer
 * - coroutine handles actually contain a pointer
 */
union TaskPromiseWaiting {
  TaskPromiseState mState {TaskPromiseState::Running};
  std::coroutine_handle<> mNext;
};
static_assert(sizeof(TaskPromiseWaiting) == sizeof(TaskPromiseState));
static_assert(sizeof(TaskPromiseWaiting) == sizeof(std::coroutine_handle<>));

constexpr TaskPromiseWaiting TaskPromiseRunning {
  .mState = TaskPromiseState::Running};
constexpr TaskPromiseWaiting TaskPromiseAbandoned {
  .mState = TaskPromiseState::Abandoned};
constexpr TaskPromiseWaiting TaskPromiseCompleted {
  .mState = TaskPromiseState::Completed};

constexpr bool operator==(TaskPromiseWaiting a, TaskPromiseWaiting b) noexcept {
  return a.mNext == b.mNext;
}

template <class TFn>
struct TaskExecutor {
  TaskExecutor() = delete;

  TaskExecutor(TFn&& fn) : mFn(std::forward<TFn>(fn)) {
  }

  TaskExecutor(TaskExecutor<TFn>&& other)
    : mOrphaned(std::exchange(other.mOrphaned, false)),
      mFn(std::move(other.mFn)) {
  }

  TaskExecutor(const TaskExecutor&) = delete;
  TaskExecutor& operator=(TaskExecutor&&) = delete;
  TaskExecutor& operator==(const TaskExecutor&) = delete;

  void operator()() {
    mOrphaned = false;
    std::invoke(mFn);
  }

  ~TaskExecutor() {
    if (mOrphaned) {
      // TODO: add and properly handle an 'Orphaned by dispatcherqueue' state
      OPENKNEEBOARD_BREAK;
    }
  }

 private:
  std::decay_t<TFn> mFn;
  bool mOrphaned = true;
};

template <class TDispatcherQueue, class TResult>
struct TaskPromiseBase;

template <class TDispatcherQueue, class TResult>
struct TaskFinalAwaiter {
  TaskPromiseBase<TDispatcherQueue, TResult>& mPromise;
  TDispatcherQueue mDQ {nullptr};

  bool await_ready() const noexcept {
    return false;
  }

  template <class TPromise>
  void await_suspend(std::coroutine_handle<TPromise> handle) noexcept {
    auto oldState = mPromise.mWaiting.exchange(TaskPromiseCompleted);
    if (oldState == TaskPromiseAbandoned) {
      mPromise.destroy();
    } else if (oldState != TaskPromiseRunning) {
      // Must have a valid pointer, or we have corruption
      assert(oldState != TaskPromiseCompleted);
      if (mDQ.HasThreadAccess()) {
        oldState.mNext.resume();
      } else {
        mDQ.TryEnqueue(
          TaskExecutor {[caller = oldState.mNext]() { caller.resume(); }});
      }
    }
  }

  void await_resume() const noexcept {
  }
};

template <class TDispatcherQueue, class TResult>
struct Task;

template <class TDispatcherQueue, class TResult>
struct TaskPromiseBase {
  std::atomic<TaskPromiseWaiting> mWaiting {TaskPromiseRunning};
  std::source_location mCaller;

  TaskPromiseBase() = delete;

  TDispatcherQueue mDQ = TDispatcherQueue::GetForCurrentThread();
  std::exception_ptr mUncaught {};

  [[msvc::noinline]]
  auto get_return_object(this auto& self) {
    return &self;
  }

  void unhandled_exception() {
    mUncaught = std::current_exception();
  }

  void abandon() {
    auto oldState = mWaiting.exchange(TaskPromiseAbandoned);
    if (oldState == TaskPromiseRunning) {
      fatal(mCaller, "result *must* be awaited");
      return;
    }
    this->destroy();
  }

  template <class Self>
  void destroy(this Self& self) {
    std::coroutine_handle<Self>::from_promise(self).destroy();
  }

  std::suspend_never initial_suspend() noexcept {
    if (!mDQ) {
      fatal(
        "Couldn't get a DispatcherQueue for the current thread - perhaps it "
        "has a DispatcherQueue from a different namespace?");
    }
    if (!mDQ.HasThreadAccess()) {
      fatal(
        "This threads dispatcher queue does not have access to this thread");
    }
    return {};
  };

  TaskFinalAwaiter<TDispatcherQueue, TResult> final_suspend() noexcept {
    return {*this, mDQ};
  }

 protected:
  TaskPromiseBase(std::source_location&& caller) : mCaller(std::move(caller)) {
  }
};

template <class TDispatcherQueue, class TResult>
struct TaskPromise : TaskPromiseBase<TDispatcherQueue, TResult> {
  TaskPromise(std::source_location&& caller = std::source_location::current())
    : TaskPromiseBase<TDispatcherQueue, TResult>(std::move(caller)) {
  }

  TResult mResult;

  void return_value(TResult&& result) noexcept {
    mResult = std::move(result);
  }
};
template <class TDispatcherQueue>
struct TaskPromise<TDispatcherQueue, void>
  : TaskPromiseBase<TDispatcherQueue, void> {
  TaskPromise(std::source_location&& caller = std::source_location::current())
    : TaskPromiseBase<TDispatcherQueue, void>(std::move(caller)) {
  }

  void return_void() noexcept {
  }
};

template <class T>
struct TaskPromiseDeleter {
  void operator()(T* promise) const noexcept {
    promise->abandon();
  }
};

template <class T>
using TaskPromisePtr = std::unique_ptr<T, TaskPromiseDeleter<T>>;

template <class TDispatcherQueue, class TResult>
struct TaskAwaiter {
  using promise_t = TaskPromise<TDispatcherQueue, TResult>;
  using promise_ptr_t = TaskPromisePtr<promise_t>;
  promise_ptr_t mPromise;

  TaskAwaiter() = delete;
  TaskAwaiter(promise_ptr_t&& init) : mPromise(std::move(init)) {
  }

  bool await_ready() const noexcept {
    switch (mPromise->mWaiting.load().mState) {
      case TaskPromiseState::Running:
        return false;
      case TaskPromiseState::Completed:
        return true;
      default:
        return false;
    }
  }

  bool await_suspend(std::coroutine_handle<> caller) {
    auto oldState = mPromise->mWaiting.exchange({.mNext = caller});
    return oldState == TaskPromiseRunning;
  }

  template <class T = TResult>
    requires std::same_as<T, void>
  void await_resume() const noexcept {
    if (mPromise->mUncaught) {
      std::rethrow_exception(std::move(mPromise->mUncaught));
    }
  }

  template <std::convertible_to<TResult> T = TResult>
    requires(!std::same_as<T, void>)
  T&& await_resume() noexcept {
    if (mPromise->mUncaught) {
      std::rethrow_exception(std::move(mPromise->mUncaught));
    }
    return std::move(mPromise->mResult);
  }
};

template <class TDispatcherQueue, class TResult>
struct [[nodiscard]] Task {
  using promise_t = TaskPromise<TDispatcherQueue, TResult>;
  using promise_ptr_t = TaskPromisePtr<promise_t>;

  using promise_type = promise_t;

  Task() = delete;
  Task(std::nullptr_t) = delete;
  Task(promise_t* promise) : mPromise(promise) {
    assert(promise);
  }

  // just to give nice compiler error messages
  struct cannot_await_lvalue_use_std_move {
    void await_ready() {
    }
  };
  cannot_await_lvalue_use_std_move operator co_await() & = delete;

  auto operator co_await() && noexcept {
    return TaskAwaiter<TDispatcherQueue, TResult> {std::move(mPromise)};
  }

  promise_ptr_t mPromise;
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {
/** A coroutine that always returns to the same thread it was invoked from.
 *
 * This is similar to wil::com_task(), except that it doesn't have as many
 * dependencies/unwanted interaections with various parts of Windows.h and
 * ole2.h
 */
template <class TDispatcherQueue, class T>
using basic_task = detail::Task<TDispatcherQueue, T>;
template <class T>
using task = basic_task<DispatcherQueue, T>;

}// namespace OpenKneeboard
