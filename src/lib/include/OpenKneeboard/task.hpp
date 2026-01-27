// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "task/TaskContextAwaiter.hpp"
#include "task/task_context.hpp"

#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>

#include <combaseapi.h>
#include <ctxtcall.h>

#include <felly/guarded_data.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
#include <wil/com.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <coroutine>

#ifndef OPENKNEEBOARD_TASK_TRACE
#define OPENKNEEBOARD_TASK_TRACE(...)
#endif

namespace OpenKneeboard::detail {

/* using `__assume(false)` instead of `std::unreachable()` throughout as - as of
 * 2026-01-26 - MSVC will raise 'unreachable code' warnings for
 * `std::unreachable()` but not for `__assume(false)`
 *
 * https://developercommunity.visualstudio.com/t/std::unreachable-causes-warning-4702-un/10720390
 */

enum class TaskAwaiting {
  Required,
  Optional,
  NotSupported,
};

enum class TaskCompletionThread {
  OriginalThread,
  AnyThread,
};

enum class TaskExceptionBehavior {
  StoreAndRethrow,
  Terminate,
};

template <class T>
concept task_traits = requires {
  typename T::result_type;
  { T::OnException } -> std::convertible_to<TaskExceptionBehavior>;
  { T::Awaiting } -> std::convertible_to<TaskAwaiting>;
  { T::CompletionThread } -> std::convertible_to<TaskCompletionThread>;
};

template <class TResult>
struct TaskTraits {
  static constexpr auto OnException = TaskExceptionBehavior::StoreAndRethrow;
  static constexpr auto Awaiting = TaskAwaiting::Required;
  static constexpr auto CompletionThread = TaskCompletionThread::OriginalThread;

  using result_type = TResult;
};
static_assert(task_traits<TaskTraits<int>>);

template <class TResult>
struct AnyThreadTaskTraits : TaskTraits<TResult> {
  static constexpr auto CompletionThread = TaskCompletionThread::AnyThread;
};
static_assert(task_traits<AnyThreadTaskTraits<int>>);

struct FireAndForgetTraits {
  static constexpr auto OnException = TaskExceptionBehavior::Terminate;
  static constexpr auto Awaiting = TaskAwaiting::NotSupported;
  static constexpr auto CompletionThread = TaskCompletionThread::AnyThread;

  using result_type = void;
};
static_assert(task_traits<FireAndForgetTraits>);

// Non-awaitable coroutine; useful as it has a handle
struct DetachedTask {
  struct promise_type {
    DetachedTask get_return_object() noexcept {
      return {std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept { std::terminate(); }
  };
  std::coroutine_handle<promise_type> mHandle;
};

enum class TaskAwaitReady {
  NotReady,
  Ready,
  ReadyWrongThread,
};

struct TaskContext : task_context {
  using task_context::mCOMCallback;
  using task_context::mThreadID;
  StackFramePointer mCaller {nullptr};

  TaskContext() = delete;
  TaskContext(StackFramePointer caller) : mCaller(std::move(caller)) {}
};

enum class TaskStateOwner : uint8_t {
  Producer = 1 << 0,// promise/coroutine
  Task = 1 << 1,
  TaskAwaiter = 1 << 2,
  ThreadPool = 1 << 3,
};

template <task_traits TTraits>
struct TaskState {
  TaskState() = delete;
  TaskState(const TaskState&) = delete;
  TaskState& operator=(const TaskState&) = delete;
  TaskState(TaskState&&) = delete;
  TaskState& operator=(TaskState&&) = delete;

  explicit TaskState(
    const TaskStateOwner initialOwner,
    const TaskContext& context)
    : mRefBits(std::to_underlying(initialOwner)),
      mContext(context) {
    OPENKNEEBOARD_TASK_TRACE(
      "TaskState()",
      TraceLoggingValue(mRefBits, "mRefBits"),
      TraceLoggingCodePointer(mContext.mCaller.value(), "caller"));
    OPENKNEEBOARD_ASSERT(initialOwner == TaskStateOwner::Producer);
  }

  ~TaskState() {
    OPENKNEEBOARD_TASK_TRACE(
      "~TaskState()", TraceLoggingValue(mRefBits, "mRefBits"));
  }

  struct exception_t {
    std::exception_ptr exception {};
    StackTrace stack;
  };
  struct void_t {};
  struct value_t {
    using value_type = std::conditional_t<
      std::is_void_v<typename TTraits::result_type>,
      void_t,
      typename TTraits::result_type>;
    FELLY_NO_UNIQUE_ADDRESS value_type value;
  };
  struct used_result_t {};

  // Monostate means moved, 'pending, or otherwise invalid
  using result_type =
    std::variant<std::monostate, exception_t, value_t, used_result_t>;
  result_type mResult {};
  TaskExceptionBehavior mOnException = TTraits::OnException;

  std::coroutine_handle<> mConsumerHandle {};

  [[nodiscard]]
  bool have_used_result() const {
    return holds_alternative<used_result_t>(mResult);
  }

  [[nodiscard]]
  constexpr TaskAwaitReady await_ready() const noexcept {
    if ((mRefBits & ThreadPoolBit)) {
      OPENKNEEBOARD_TASK_TRACE("TaskState::await_ready/threadPool");
      return TaskAwaitReady::NotReady;
    }

    // What does it mean for coroutine to be "finished"?
    //
    // The coroutine (producer) returning isn't enough - we need it to
    // completely finish, including any relevant destructors after the
    // `co_return`, e.g.:
    //
    // task<int> foo() {
    //    auto guard = scope_exit([] { my_cleanup_function(); });
    //    bar();
    //    co_return 123;
    // }
    //
    // 1. bar() is called
    // 2. co_return stores `123` as the return value of the coroutine
    // 3. scope_exit's destructor is called
    // 4. scope_exit's destructor calls my_cleanup_function();
    // 5. coroutine is now finished; the return value can be retrieved with
    //   `co_await`
    //
    // auto x = co_await foo();
    //        ^ we want to guarantee that ~scope_exit() and bar() are called
    //          before this assignment
    if ((mRefBits & ProducerBit)) {
      OPENKNEEBOARD_TASK_TRACE("TaskState::await_ready/producer");
      return TaskAwaitReady::NotReady;
    }
    // If we reach here, the coroutine's finished - but we need to still have a
    // stored value (from co_return) or uncaught exception. We should
    // *always* have one here because:
    // - if the state is pending, we should still have a producer
    // - all tasks must be awaited or moved; they can't be copied
    // - we only reach the 'result used' state after a co_await; as we can't be
    //   awaited twice, we should never have the 'result used' state when
    //   the compiler is checking if co_await is blocking, as that would be a
    //   double-await
    OPENKNEEBOARD_ASSERT(
      holds_alternative<value_t>(mResult)
        || holds_alternative<exception_t>(mResult),
      "task coroutine has completed, but we don't have a stored result or "
      "exception; perhaps a `co_return` is missing?");

    if constexpr (
      TTraits::CompletionThread == TaskCompletionThread::AnyThread) {
      OPENKNEEBOARD_TASK_TRACE("TaskState::await_ready/anyThread");
      return TaskAwaitReady::Ready;
    } else {
      if (mContext.is_this_thread()) {
        OPENKNEEBOARD_TASK_TRACE("TaskState::await_ready/ready");
        return TaskAwaitReady::Ready;
      }
      OPENKNEEBOARD_TASK_TRACE("TaskState::await_ready/readyWrongThread");
      return TaskAwaitReady::ReadyWrongThread;
    }
  }

  void return_void() {
    OPENKNEEBOARD_ASSERT(holds_alternative<std::monostate>(mResult));
    mResult.template emplace<value_t>();
  }

  template <class U = typename TTraits::result_type>
    requires(!std::is_void_v<std::remove_cvref_t<U>>)
  void return_value(U&& result) {
    OPENKNEEBOARD_ASSERT(holds_alternative<std::monostate>(mResult));
    mResult.template emplace<value_t>(std::forward<U>(result));
  }

  void store_current_exception() {
    OPENKNEEBOARD_ASSERT(holds_alternative<std::monostate>(mResult));
    switch (mOnException) {
      case TaskExceptionBehavior::StoreAndRethrow:
        mResult = exception_t {
          std::current_exception(),
          StackTrace::GetForMostRecentException(),
        };
        return;
      case TaskExceptionBehavior::Terminate:
        fatal_with_exception(std::current_exception());
    }
    __assume(0);
  }

  // Returns true if TOwner was the last owner, and the state should be deleted.
  template <TaskStateOwner TOwner>
  [[nodiscard]] bool release() noexcept {
    constexpr auto bits = std::to_underlying(TOwner);
    static_assert(std::popcount(bits) == 1);
    OPENKNEEBOARD_ASSERT((mRefBits & bits) == bits);
    if constexpr (bits & ThreadPoolBit) {
      OPENKNEEBOARD_ASSERT(!(mRefBits & ProducerBit));
    }
    mRefBits &= ~bits;
    OPENKNEEBOARD_TASK_TRACE(

      "TaskState::release",
      TraceLoggingValue(mRefBits, "mRefBits"),
      TraceLoggingValue(magic_enum::enum_name(TOwner).data(), "removed"));
    return !mRefBits;
  }

  template <TaskStateOwner TOwner>
  void add_ref() noexcept {
    constexpr auto bits = std::to_underlying(TOwner);
    static_assert(std::popcount(bits) == 1);
    OPENKNEEBOARD_ASSERT(!(mRefBits & bits));
    mRefBits |= bits;
    OPENKNEEBOARD_TASK_TRACE(

      "TaskState::add_ref",
      TraceLoggingValue(mRefBits, "mRefBits"),
      TraceLoggingValue(magic_enum::enum_name(TOwner).data(), "added"));
  }

  task_context get_task_context() const noexcept { return mContext; }

 private:
  static constexpr uint8_t ProducerBit =
    std::to_underlying(TaskStateOwner::Producer);
  static constexpr uint8_t TaskBit = std::to_underlying(TaskStateOwner::Task);
  static constexpr uint8_t TaskAwaiterBit =
    std::to_underlying(TaskStateOwner::TaskAwaiter);
  static constexpr uint8_t ThreadPoolBit =
    std::to_underlying(TaskStateOwner::ThreadPool);
  uint8_t mRefBits {};

  TaskContext mContext;
};

template <task_traits TTraits, TaskStateOwner TOwner>
struct TaskStatePtr {
  template <task_traits T, TaskStateOwner U>
  friend struct TaskStatePtr;

  using value_type = felly::guarded_data<TaskState<TTraits>>;
  using lock_type = std::remove_cvref_t<decltype(value_type {}.lock())>;
  using pointer_type = value_type*;

  TaskStatePtr() = delete;
  template <class... Args>
  explicit TaskStatePtr(std::in_place_t, Args&&... args)
    requires(TOwner == TaskStateOwner::Producer)
    : mImpl(new value_type {TOwner, std::forward<Args>(args)...}) {}

  constexpr ~TaskStatePtr() { this->reset(); }

  void reset() {
    if (!mImpl) {
      return;
    }
    reset(mImpl->lock());
  }

  void reset(lock_type lock) {
    const auto ptr = std::exchange(mImpl, nullptr);
    OPENKNEEBOARD_ASSERT(ptr);

    if (!lock->template release<TOwner>()) {
      return;
    }
    if constexpr (TTraits::Awaiting == TaskAwaiting::Required) {
      OPENKNEEBOARD_ASSERT(
        lock->have_used_result(),
        "all tasks must be awaited (or moved-then-awaited)");
    }

    lock.unlock();
    delete ptr;
  }

  template <TaskStateOwner TOtherOwner>
    requires(TOtherOwner != TOwner)
  TaskStatePtr(const TaskStatePtr<TTraits, TOtherOwner>& other) {
    OPENKNEEBOARD_ASSERT(other);
    other.lock()->template add_ref<TOwner>();
    mImpl = other.mImpl;
  }

  template <TaskStateOwner TOtherOwner>
    requires(TOtherOwner != TOwner)
  TaskStatePtr(
    const TaskStatePtr<TTraits, TOtherOwner>& other,
    lock_type& lock) {
    OPENKNEEBOARD_ASSERT(other);
    mImpl = other.mImpl;
    lock->template add_ref<TOwner>();
  }

  template <TaskStateOwner TOtherOwner>
    requires(TOtherOwner != TOwner)
  auto copied_to(lock_type lock) {
    return TaskStatePtr<TTraits, TOtherOwner> {*this, lock};
  }

  template <TaskStateOwner TOtherOwner>
    requires(TOtherOwner != TOwner)
  auto moved_to(lock_type lock) {
    return TaskStatePtr<TTraits, TOtherOwner> {std::move(*this), lock};
  }

  TaskStatePtr(TaskStatePtr&& other) noexcept {
    mImpl = std::exchange(other.mImpl, nullptr);
  }

  constexpr operator bool() const noexcept { return mImpl; }

  auto lock() const {
    OPENKNEEBOARD_ASSERT(mImpl);
    return mImpl->lock();
  }

  TaskStatePtr(const TaskStatePtr&) = delete;
  TaskStatePtr& operator=(const TaskStatePtr&) = delete;
  TaskStatePtr& operator=(TaskStatePtr&&) = delete;
  TaskStatePtr& operator=(std::nullptr_t) noexcept {
    this->reset();
    return *this;
  }

 private:
  pointer_type mImpl {nullptr};
};

template <task_traits TTraits>
[[nodiscard]]
DetachedTask ResumeOnOriginalThread(
  TaskStatePtr<TTraits, TaskStateOwner::ThreadPool> state) noexcept {
  OPENKNEEBOARD_ASSERT(state);
  auto context = state.lock()->get_task_context();
  OPENKNEEBOARD_ASSERT(!context.is_this_thread());
  co_await TaskContextAwaiter {context};

  OPENKNEEBOARD_ASSERT(state);
  auto lock = state.lock();
  const auto handle = std::exchange(lock->mConsumerHandle, {});
  OPENKNEEBOARD_ASSERT(handle);
  state.reset(std::move(lock));

  handle.resume();
  co_return;
}

template <task_traits TTraits>
struct TaskPromiseBase;

template <task_traits TTraits>
struct TaskPromise;

/// Sentinel marker type handled by await_transform
struct noexcept_task_t {};

template <task_traits TTraits>
struct TaskFinalAwaiter {
  TaskPromiseBase<TTraits>& mPromise;

  TaskFinalAwaiter() = delete;
  TaskFinalAwaiter(TaskPromiseBase<TTraits>& promise) : mPromise(promise) {}

  bool await_ready() const noexcept { return false; }

  template <std::derived_from<TaskPromiseBase<TTraits>> TPromise>
  std::coroutine_handle<> await_suspend(
    std::coroutine_handle<TPromise> producer) noexcept {
    OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend");
    auto state = std::move(producer.promise().mState);

    OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend/acquiringLock");
    auto lock = state.lock();
    OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend/locked");
    const auto handle = lock->mConsumerHandle;
    const auto context = lock->get_task_context();
    OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend/unlocked");

    auto releaseStateGuard = scope_exit([&] {
      // We need the refbit removed before the lock is released
      state.reset(std::move(lock));
    });

    if (!handle) {
      OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend/nullHandle");
      // We need the refcount removed before the lock is released
      return [](auto destroy) -> DetachedTask {
        destroy.destroy();
        co_return;
      }(producer)
                                   .mHandle;
    }

    if constexpr (
      TTraits::CompletionThread == TaskCompletionThread::AnyThread) {
      OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend/anyThread");
      return [](auto toDestroy, auto toResume) -> DetachedTask {
        toDestroy.destroy();
        toResume.resume()();
        co_return;
      }(producer, handle)
                                                    .mHandle;
    } else {
      if (context.is_this_thread()) {
        OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend/thisThread");
        return [](auto toDestroy, auto toResume) -> DetachedTask {
          toDestroy.destroy();
          toResume.resume();
          co_return;
        }(producer, handle)
                                                      .mHandle;
      }

      OPENKNEEBOARD_TASK_TRACE("TaskFinalAwaiter::await_suspend/wrongThread");

      // We're stealing the lock back
      releaseStateGuard.release();
      return [](auto toDestroy, auto toResume) -> DetachedTask {
        toDestroy.destroy();
        toResume.resume();
        co_return;
      }(producer,
        ResumeOnOriginalThread<TTraits>(
          state.template moved_to<TaskStateOwner::ThreadPool>(std::move(lock)))
          .mHandle)
                                                    .mHandle;
    }
  }

  void await_resume() const noexcept {}
};

template <task_traits TTraits>
struct Task;

template <task_traits TTraits>
struct TaskPromiseBase {
  using traits_type = TTraits;

  TaskContext mContext;
  const std::coroutine_handle<> mProducerHandle;
  TaskStatePtr<TTraits, TaskStateOwner::Producer> mState;

  TaskPromiseBase() = delete;
  TaskPromiseBase(const TaskPromiseBase<TTraits>&) = delete;
  TaskPromiseBase<TTraits>& operator=(const TaskPromiseBase<TTraits>&) = delete;

  TaskPromiseBase(TaskPromiseBase<TTraits>&&) = delete;
  TaskPromiseBase<TTraits>& operator=(TaskPromiseBase<TTraits>&&) = delete;

  TaskPromiseBase(
    TaskContext&& context,
    const std::coroutine_handle<> self) noexcept
    : mContext(context),
      mProducerHandle(self),
      mState(std::in_place, std::move(context)) {}

  ~TaskPromiseBase() {}

  auto get_return_object() {
    OPENKNEEBOARD_TASK_TRACE("TaskPromiseBase::get_return_object");
    return mState.template copied_to<TaskStateOwner::Task>(mState.lock());
  }

  void unhandled_exception() noexcept {
    mState.lock()->store_current_exception();
  }

  constexpr std::suspend_never initial_suspend() noexcept { return {}; };

  TaskFinalAwaiter<TTraits> final_suspend() noexcept { return {*this}; }

  template <class TAwaitable>
  TAwaitable&& await_transform(TAwaitable&& it) noexcept {
    return static_cast<TAwaitable&&>(it);
  }

  auto await_transform(noexcept_task_t) noexcept {
    mState.lock()->mOnException = TaskExceptionBehavior::Terminate;
    return std::suspend_never {};
  }
};

template <task_traits TTraits>
struct TaskPromise : TaskPromiseBase<TTraits> {
  TaskPromise(std::optional<StackFramePointer> caller = std::nullopt)
    : TaskPromiseBase<TTraits>(
        caller.value_or(StackFramePointer::caller(2)),
        std::coroutine_handle<TaskPromise<TTraits>>::from_promise(*this)) {}

  void return_value(typename TTraits::result_type&& result) noexcept {
    this->mState.lock()->return_value(std::move(result));
  }
};

template <task_traits TTraits>
  requires std::same_as<typename TTraits::result_type, void>
struct TaskPromise<TTraits> : TaskPromiseBase<TTraits> {
  TaskPromise(std::optional<StackFramePointer> caller = std::nullopt)
    : TaskPromiseBase<TTraits>(
        caller.value_or(StackFramePointer::caller(2)),
        std::coroutine_handle<TaskPromise<TTraits>>::from_promise(*this)) {}

  void return_void() noexcept { this->mState.lock()->return_void(); }
};

template <task_traits TTraits>
struct TaskAwaiter {
  using promise_t = TaskPromise<TTraits>;
  TaskStatePtr<TTraits, TaskStateOwner::TaskAwaiter> mState;

  TaskAwaiter() = delete;
  TaskAwaiter(const TaskAwaiter&) = delete;
  TaskAwaiter& operator=(const TaskAwaiter&) = delete;

  TaskAwaiter(TaskAwaiter&&) = delete;
  TaskAwaiter& operator=(TaskAwaiter&&) = delete;

  TaskAwaiter(TaskStatePtr<TTraits, TaskStateOwner::TaskAwaiter> state) noexcept
    : mState(std::move(state)) {}
  ~TaskAwaiter() = default;

  [[nodiscard]]
  bool await_ready() const noexcept {
    OPENKNEEBOARD_ASSERT(mState, "Can't await a null promise");
    return mState.lock()->await_ready() == TaskAwaitReady::Ready;
  }

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) {
    OPENKNEEBOARD_TASK_TRACE("TaskAwaiter::await_suspend");
    OPENKNEEBOARD_ASSERT(mState, "Can't await a null promise");
    OPENKNEEBOARD_TASK_TRACE("TaskAwaiter::await_suspend/locking");
    auto lock = mState.lock();
    OPENKNEEBOARD_TASK_TRACE("TaskAwaiter::await_suspend/locked");

    OPENKNEEBOARD_ASSERT(
      !lock->mConsumerHandle, "tasks can only be resumed once");

    using enum TaskAwaitReady;
    switch (lock->await_ready()) {
      case Ready:
        OPENKNEEBOARD_TASK_TRACE("TaskAwaiter::await_suspend/ready");
        return caller;
      case NotReady:
        OPENKNEEBOARD_TASK_TRACE("TaskAwaiter::await_suspend/notReady");
        lock->mConsumerHandle = caller;
        return std::noop_coroutine();
      case ReadyWrongThread:
        OPENKNEEBOARD_TASK_TRACE("TaskAwaiter::await_suspend/readyWrongThread");
        lock->mConsumerHandle = caller;
        OPENKNEEBOARD_ASSERT(caller);
        return ResumeOnOriginalThread(
                 mState.template copied_to<TaskStateOwner::ThreadPool>(
                   std::move(lock)))
          .mHandle;
    }
    __assume(false);
  }

  auto await_resume() {
    OPENKNEEBOARD_ASSERT(
      mState, "can't resume a moved or already-resumed task");

    auto state = mState.lock();

    OPENKNEEBOARD_ASSERT(state->await_ready() == TaskAwaitReady::Ready);

    using value_type = typename TaskState<TTraits>::value_t;
    using exception_t = typename TaskState<TTraits>::exception_t;
    using used_result_t = typename TaskState<TTraits>::used_result_t;

    auto result = std::exchange(state->mResult, used_result_t {});
    state.unlock();
    mState = nullptr;

    if (auto p = get_if<exception_t>(&result)) {
      StackTrace::SetForNextException(std::move(p->stack));
      std::rethrow_exception(p->exception);
    }

    if constexpr (!std::same_as<typename TTraits::result_type, void>) {
      return std::move(get<value_type>(result).value);
    }
  }
};

template <task_traits TTraits>
struct [[nodiscard]] Task {
  static constexpr bool must_await_v =
    TTraits::Awaiting == TaskAwaiting::Required;

  using promise_t = TaskPromise<TTraits>;
  using promise_ptr_t = promise_t*;

  using promise_type = promise_t;

  Task() = delete;
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  [[nodiscard]]
  constexpr bool is_moved() const noexcept {
    return !mState;
  }

  Task(Task&& other) noexcept { mState = std::exchange(other.mState, {}); }

  Task& operator=(Task&& other) noexcept {
    mState = std::exchange(other.mState, nullptr);
    return *this;
  }

  Task(std::nullptr_t) = delete;
  Task(TaskStatePtr<TTraits, TaskStateOwner::Task> state)
    : mState(std::move(state)) {
    OPENKNEEBOARD_ASSERT(mState);
  }

  ~Task() {}

  // just to give nice compiler error messages
  struct cannot_await_lvalue_use_std_move {
    void await_ready() {}
  };
  cannot_await_lvalue_use_std_move operator co_await() & = delete;

  auto operator co_await() && noexcept
    requires(TTraits::Awaiting != TaskAwaiting::NotSupported)
  {
    OPENKNEEBOARD_ASSERT(
      mState, "Can't await a task that has been moved or already awaited");
    return TaskAwaiter<TTraits> {std::move(mState)};
  }

  TaskStatePtr<TTraits, TaskStateOwner::Task> mState {nullptr};
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard::inline task_ns {
/** A coroutine that:
 * - always returns to the same thread it was invoked from
 * - to implement that, requires that it is called from a thread with a COM
 * apartment
 * - calls fatal() if not awaited
 * - statically requires that the result is discarded via [[nodiscard]]
 * - calls fatal() if there is an uncaught exception
 * - propagates uncaught exceptions back to the caller
 *
 * This is similar to wil::com_task(), except that it doesn't have as many
 * dependencies/unwanted interactions with various parts of Windows.h and
 * ole2.h
 *
 * If you want to store one, use an `std::optional<task<T>>`; to co_await the
 * stored task, use `co_await std::move(optional).value()`
 */
template <class T>
using task = detail::Task<detail::TaskTraits<T>>;

template <class T>
using any_thread_task = detail::Task<detail::AnyThreadTaskTraits<T>>;

struct fire_and_forget : detail::Task<detail::FireAndForgetTraits> {
  using detail::Task<detail::FireAndForgetTraits>::Task;

  static fire_and_forget wrap(auto toWrap, auto... args) {
    co_await std::invoke(std::move(toWrap), std::move(args)...);
    co_return;
  }
};

namespace this_task {
/** Call `OpenKneeboard::fatal()` if this task throws an exception.
 *
 * Usage: `co_await this_task::fatal_on_uncaught_exception()`.
 *
 * This is better than `noexcept` because more information can be preserved
 * (and logged) about the exception. This allow allows the
 * 'fatal-on-exception' behavior to be considered an implementation detail of
 * the function, instead of part of the function signature.
 */
[[nodiscard]]
constexpr detail::noexcept_task_t fatal_on_uncaught_exception() noexcept {
  return {};
}

// Suspend the coroutine (not the thread)
//
// usage: `co_await this_task::yield()`
[[nodiscard]]
inline auto yield() noexcept {
  struct suspend_once {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) const noexcept {
      handle.resume();
    }
    void await_resume() const noexcept {}
  };
  return suspend_once {};
}

}// namespace this_task

}// namespace OpenKneeboard::inline task_ns
