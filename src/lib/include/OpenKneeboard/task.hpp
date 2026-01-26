// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

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

#include <atomic>
#include <coroutine>
#include <memory>

namespace OpenKneeboard::detail {

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

enum class TaskCompletionThread {
  OriginalThread,
  AnyThread,
};

struct TaskContext {
  wil::com_ptr<IContextCallback> mCOMCallback;
  std::thread::id mThreadID = std::this_thread::get_id();
  StackFramePointer mCaller {nullptr};

  inline TaskContext(StackFramePointer&& caller) : mCaller(std::move(caller)) {
    if (!SUCCEEDED(CoGetObjectContext(IID_PPV_ARGS(mCOMCallback.put()))))
      [[unlikely]] {
      this->fatal("Attempted to create a task<> from thread without COM");
    }
  }

  template <class... Ts>
  [[noreturn]]
  void fatal(std::format_string<Ts...> fmt, Ts&&... values) const noexcept {
    const auto message = std::format(fmt, std::forward<Ts>(values)...);
    OpenKneeboard::fatal(
      mCaller,
      "{}\nCaller: {}\nCaller thread: {}\nFinal thread: {}",
      message,
      mCaller.to_string(),
      mThreadID,
      std::this_thread::get_id());
  }
};

enum class TaskExceptionBehavior {
  StoreAndRethrow,
  Terminate,
};

template <class TTraits>
struct TaskState {
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
  constexpr bool await_ready() const noexcept {
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
      return false;
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
      || holds_alternative<exception_t>(mResult));
    return true;
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

  // Returns true if consumer should delete, i.e. if the producer had already
  // completed and detached
  //
  // If true, the consumer was the last owner
  [[nodiscard]] bool release_consumer() {
    OPENKNEEBOARD_ASSERT(mRefBits & ConsumerBit);
    mRefBits &= ~ConsumerBit;
    return !mRefBits;
  }

  // Returns true if producer should delete, i.e. we're an
  // orphaned/detached/fire_and_forget coroutine
  //
  // If true, the producer was the last co-owner.
  [[nodiscard]] bool release_producer() {
    OPENKNEEBOARD_ASSERT(mRefBits & ProducerBit);
    mRefBits &= ~ProducerBit;
    return !mRefBits;
  }

 private:
  static constexpr uint8_t ProducerBit = 1;
  static constexpr uint8_t ConsumerBit = 2;
  uint8_t mRefBits {ProducerBit | ConsumerBit};
};

template <class TTraits>
struct TaskPromiseBase;

template <class TTraits>
struct TaskPromise;

enum class TaskAwaiting {
  Required,
  Optional,
  NotSupported,
};

template <class TResult>
struct TaskTraits {
  static constexpr auto OnException = TaskExceptionBehavior::StoreAndRethrow;
  static constexpr auto Awaiting = TaskAwaiting::Required;
  static constexpr auto CompletionThread = TaskCompletionThread::OriginalThread;

  using result_type = TResult;
};

template <class TResult>
struct AnyThreadTaskTraits : TaskTraits<TResult> {
  static constexpr auto CompletionThread = TaskCompletionThread::AnyThread;
};

struct FireAndForgetTraits {
  static constexpr auto OnException = TaskExceptionBehavior::Terminate;
  static constexpr auto Awaiting = TaskAwaiting::NotSupported;
  static constexpr auto CompletionThread = TaskCompletionThread::AnyThread;

  using result_type = void;
};

/// Sentinel marker type handled by await_transform
struct noexcept_task_t {};

template <class TTraits>
struct TaskFinalAwaiter {
  TaskPromiseBase<TTraits>& mPromise;

  TaskFinalAwaiter() = delete;
  TaskFinalAwaiter(TaskPromiseBase<TTraits>& promise) : mPromise(promise) {}

  bool await_ready() const noexcept { return false; }

  template <std::derived_from<TaskPromiseBase<TTraits>> TPromise>
  std::coroutine_handle<> await_suspend(
    std::coroutine_handle<TPromise> producer) noexcept {
    auto& promise = producer.promise();
    auto state = promise.mState.lock();

    if constexpr (
      TTraits::CompletionThread == TaskCompletionThread::AnyThread) {
      return promise.resume_on_this_thread(std::move(state));
    } else {
      if (promise.mContext.mThreadID == std::this_thread::get_id()) {
        return promise.resume_on_this_thread(std::move(state));
      }

      return [](TaskPromiseBase<TTraits>* const promise) -> DetachedTask {
        TaskFinalAwaiter::bounce_to_thread_pool(promise);
        co_return;
      }(&promise)
                                                              .mHandle;
    }
  }

  void await_resume() const noexcept {}

 private:
  static void bounce_to_thread_pool(
    TaskPromiseBase<TTraits>* promise) noexcept {
    const auto threadPoolSuccess = TrySubmitThreadpoolCallback(
      &TaskFinalAwaiter<TTraits>::thread_pool_callback, promise, nullptr);

    if (threadPoolSuccess) [[likely]] {
      return;
    }
    const auto threadPoolError = GetLastError();
    promise->mContext.fatal(
      "Failed to enqueue resumption on thread pool: {:010x}",
      static_cast<uint32_t>(threadPoolError));
    __assume(false);
  }

  static void thread_pool_callback(
    PTP_CALLBACK_INSTANCE,
    void* userData) noexcept {
    auto promise = static_cast<TaskPromiseBase<TTraits>*>(userData);

    ComCallData comData {.pUserDefined = userData};
    const auto result = promise->mContext.mCOMCallback->ContextCallback(
      &TaskFinalAwaiter<TTraits>::target_thread_callback,
      &comData,
      IID_ICallbackWithNoReentrancyToApplicationSTA,
      5,
      NULL);
    if (SUCCEEDED(result)) [[likely]] {
      return;
    }
    promise->mContext.fatal(
      "Failed to enqueue coroutine resumption for the desired thread: "
      "{:#010x}",
      static_cast<uint32_t>(result));
  }

  static HRESULT target_thread_callback(ComCallData* comData) noexcept {
    const auto promise =
      static_cast<TaskPromiseBase<TTraits>*>(comData->pUserDefined);

    try {
      promise->resume_on_this_thread(promise->mState.lock()).resume();
    } catch (...) {
      dprint.Warning("std::coroutine_handle<>::resume() threw an exception");
      fatal_with_exception(std::current_exception());
    }

    return S_OK;
  }
};

template <class TTraits>
struct Task;

template <class TTraits>
struct TaskPromiseBase {
  using traits_type = TTraits;

  TaskContext mContext;
  const std::coroutine_handle<> mProducerHandle;
  felly::guarded_data<TaskState<TTraits>> mState;

  TaskPromiseBase() = delete;
  TaskPromiseBase(const TaskPromiseBase<TTraits>&) = delete;
  TaskPromiseBase<TTraits>& operator=(const TaskPromiseBase<TTraits>&) = delete;

  TaskPromiseBase(TaskPromiseBase<TTraits>&&) = delete;
  TaskPromiseBase<TTraits>& operator=(TaskPromiseBase<TTraits>&&) = delete;

  TaskPromiseBase(
    TaskContext&& context,
    const std::coroutine_handle<> self) noexcept
    : mContext(std::move(context)),
      mProducerHandle(self) {}

  ~TaskPromiseBase() {}

  auto get_return_object() { return static_cast<TaskPromise<TTraits>*>(this); }

  void unhandled_exception() noexcept {
    mState.lock()->store_current_exception();
  }

  void consumer_destroyed() {
    auto state = mState.lock();

    if constexpr (TTraits::Awaiting == TaskAwaiting::Required) {
      OPENKNEEBOARD_ASSERT(
        state->have_used_result(), "tasks must be moved or awaited");
    }

    if (state->release_consumer()) {
      state.unlock();
      mProducerHandle.destroy();
    }
  }

  [[nodiscard]]
  std::coroutine_handle<> resume_on_this_thread(
    felly::unique_guarded_data_lock<TaskState<TTraits>> state) noexcept {
    if (state->release_producer()) {
      return [](std::coroutine_handle<> producer) -> DetachedTask {
        producer.destroy();
        co_return;
      }(mProducerHandle)
                                                       .mHandle;
    }

    // Still have a consumer...
    // ... that is waiting
    if (state->mConsumerHandle) {
      return state->mConsumerHandle;
    }
    // ... that is not yet waiting
    return std::noop_coroutine();
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

template <class TTraits>
struct TaskPromise : TaskPromiseBase<TTraits> {
  TaskPromise(std::optional<StackFramePointer> caller = std::nullopt)
    : TaskPromiseBase<TTraits>(
        caller.value_or(StackFramePointer::caller(2)),
        std::coroutine_handle<TaskPromise<TTraits>>::from_promise(*this)) {}

  void return_value(typename TTraits::result_type&& result) noexcept {
    this->mState.lock()->return_value(std::move(result));
  }
};

template <class TTraits>
  requires std::same_as<typename TTraits::result_type, void>
struct TaskPromise<TTraits> : TaskPromiseBase<TTraits> {
  TaskPromise(std::optional<StackFramePointer> caller = std::nullopt)
    : TaskPromiseBase<TTraits>(
        caller.value_or(StackFramePointer::caller(3)),
        std::coroutine_handle<TaskPromise<TTraits>>::from_promise(*this)) {}

  void return_void() noexcept { this->mState.lock()->return_void(); }
};

template <class TTraits>
struct TaskAwaiter {
  using promise_t = TaskPromise<TTraits>;
  promise_t* mPromise;

  TaskAwaiter() = delete;
  TaskAwaiter(const TaskAwaiter&) = delete;
  TaskAwaiter& operator=(const TaskAwaiter&) = delete;

  TaskAwaiter(TaskAwaiter&&) = delete;
  TaskAwaiter& operator=(TaskAwaiter&&) = delete;

  TaskAwaiter(promise_t* init) : mPromise(init) {
    OPENKNEEBOARD_ASSERT(mPromise, "Can't await a null promise");
  }

  [[nodiscard]]
  bool await_ready() const noexcept {
    OPENKNEEBOARD_ASSERT(mPromise, "Can't await a null promise");
    return mPromise->mState.lock()->await_ready();
  }

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) {
    OPENKNEEBOARD_ASSERT(mPromise, "Can't await a null promise");
    auto state = mPromise->mState.lock();
    if (state->await_ready()) {
      return caller;
    }
    OPENKNEEBOARD_ASSERT(
      !state->mConsumerHandle, "tasks can only be resumed once");
    state->mConsumerHandle = caller;
    return std::noop_coroutine();
  }

  auto await_resume() {
    OPENKNEEBOARD_ASSERT(
      mPromise, "can't resume a moved or already-resumed task");

    auto state = mPromise->mState.lock();

    OPENKNEEBOARD_ASSERT(state->await_ready());

    using value_type = typename TaskState<TTraits>::value_t;
    using exception_t = typename TaskState<TTraits>::exception_t;
    using used_result_t = typename TaskState<TTraits>::used_result_t;

    auto result = std::exchange(state->mResult, used_result_t {});

    /* We must *fully* clean up the producer before we resume, otherwise
     * parameter destructors won't be called, and the parameters will be leaked.
     *
     * For example, in this code...
     *
     *     task<int> foo(auto destroyedAfterReturn) {
     *       co_return 123;
     *     };
     *
     *     auto x = co_await foo(scope_exit([] { bar(); }));
     *
     *  ... we need ~scope_exit() - and bar() - to be called before co_await
     *  completes, and before the assignment
     */
    const auto dispose = state->release_consumer();
    OPENKNEEBOARD_ASSERT(
      dispose,
      "TaskAwaiter::await_resume() should never be called while the coroutine "
      "is still running");
    if (dispose) [[likely]] {
      state.unlock();
      std::exchange(mPromise, nullptr)->mProducerHandle.destroy();
    }

    if (auto p = get_if<exception_t>(&result)) {
      StackTrace::SetForNextException(std::move(p->stack));
      std::rethrow_exception(p->exception);
    }

    if constexpr (!std::same_as<typename TTraits::result_type, void>) {
      return std::move(get<value_type>(result).value);
    }
  }
};

template <class TTraits>
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
    return !mPromise;
  }

  Task(Task&& other) noexcept {
    mPromise = std::exchange(other.mPromise, nullptr);
  }

  Task& operator=(Task&& other) noexcept {
    mPromise = std::exchange(other.mPromise, nullptr);
    return *this;
  }

  Task(std::nullptr_t) = delete;
  Task(promise_t* promise) : mPromise(promise) {
    OPENKNEEBOARD_ASSERT(promise);
  }

  ~Task() {
    if (mPromise) {
      mPromise->consumer_destroyed();
    }
  }

  // just to give nice compiler error messages
  struct cannot_await_lvalue_use_std_move {
    void await_ready() {}
  };
  cannot_await_lvalue_use_std_move operator co_await() & = delete;

  auto operator co_await() && noexcept
    requires(TTraits::Awaiting != TaskAwaiting::NotSupported)
  {
    OPENKNEEBOARD_ASSERT(
      mPromise, "Can't await a task that has been moved or already awaited");
    // We're an rvalue-reference (move), so we should wipe our promise
    return TaskAwaiter<TTraits> {std::exchange(mPromise, nullptr)};
  }

  promise_ptr_t mPromise {};
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
}// namespace this_task

}// namespace OpenKneeboard::inline task_ns
