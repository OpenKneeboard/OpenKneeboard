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
#include <OpenKneeboard/format/enum.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <combaseapi.h>
#include <ctxtcall.h>

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

constexpr bool DebugTaskCoroutines = true;

/* The state of the Task, unless there's a pending `co_await` on the task
 * itself.
 *
 * If there is a pending co_await on the task, TaskPromiseWaiting contains the
 * coroutine handle to resume on completion, otherwise it stores one of these
 * values.
 *
 * If someone has odd-aligned pointers, they deserve everything that happens to
 * them. As we're assuming odd-aligned pointers, we can use the LSB as a 'not a
 * pointer' flag
 */
enum class DetachedState : uintptr_t {
  ProducerPending = 0,
  ProducerCompleting = (1 << 1) | 1,
  ProducerResumingConsumer = (2 << 1) | 1,
  ProducerCompleted = (3 << 1) | 1,
  ConsumerResumed = (4 << 1) | 1,
  // Coroutine complete by the time await_ready is called, consumer is never
  // suspended (await_ready returns true)
  ReadyWithoutSuspend = (5 << 1) | 1,
  // While typically this is reached after ConsumerResumed or
  // ReadyWithoutSuspend, in the case of abandoned tasks (e.g. fire_and_forget),
  // it can be set at any earlier point
  ConsumerDestroyed = (6 << 1) | 1,
};

enum class TaskPromiseResultState {
  Invalid = 0,// Moved or never initialized
  NoResult,

  HaveException,
  ThrownException,

  HaveResult,
  ReturnedResult,

  HaveVoidResult,
  ReturnedVoid,
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

/* Functions as a union so we can do std::atomic.
 *
 * - DetachedState is sizeof(ptr), but always an invalid pointer
 * - coroutine handles actually contain a pointer
 */
struct TaskPromiseWaiting {
  constexpr TaskPromiseWaiting() = default;
  constexpr TaskPromiseWaiting(const DetachedState state)
    : mStorage {std::to_underlying(state)} {}

  constexpr TaskPromiseWaiting(std::coroutine_handle<> consumer)
    : mStorage(std::bit_cast<uintptr_t>(consumer.address())) {
    OPENKNEEBOARD_ASSERT(
      (mStorage & 1) == 0,
      "pointer with set LSB is unsupported, as LSB is used as discriminator "
      "bit");
    OPENKNEEBOARD_ASSERT(mStorage, "nullptr isn't a valid coroutine handle");
  }

  constexpr bool IsDetached() const {
    if (mStorage == 0) {
      static_assert(std::to_underlying(DetachedState::ProducerPending) == 0);
      return true;
    }
    if ((mStorage & 1)) {
      OPENKNEEBOARD_ASSERT(
        magic_enum::enum_contains<DetachedState>(mStorage),
        "discriminator indicates DetachedState, but {} is not a member of "
        "DetachedState",
        mStorage);
      return true;
    }
    return false;
  }

  constexpr bool ContainsConsumerHandle() const { return !IsDetached(); }

  constexpr DetachedState GetDetachedState() const {
    OPENKNEEBOARD_ASSERT(IsDetached());
    return static_cast<DetachedState>(mStorage);
  }

  constexpr std::coroutine_handle<> GetConsumerHandle() const {
    OPENKNEEBOARD_ASSERT(ContainsConsumerHandle());
    return std::coroutine_handle<>::from_address(
      std::bit_cast<void*>(mStorage));
  }

  constexpr bool operator==(const TaskPromiseWaiting&) const = default;

  enum class Owner { Producer, Consumer };

  [[nodiscard]] constexpr Owner GetOwner() const noexcept {
    if (ContainsConsumerHandle()) {
      return Owner::Consumer;
    }
    using enum DetachedState;
    switch (GetDetachedState()) {
      case ConsumerResumed:
      case ReadyWithoutSuspend:
      case ProducerResumingConsumer:
      case ProducerCompleted:
        return Owner::Consumer;
      case ProducerPending:
      case ConsumerDestroyed:
      case ProducerCompleting:
        return Owner::Producer;
    }
    __assume(false);
  }

 private:
  uintptr_t mStorage {std::to_underlying(DetachedState::ProducerPending)};
};
static_assert(
  std::is_trivially_copyable_v<TaskPromiseWaiting>,
  "TaskPromiseWaiting must be trivially copyable for use with std::atomic");
static_assert(sizeof(TaskPromiseWaiting) == sizeof(DetachedState));
static_assert(sizeof(TaskPromiseWaiting) == sizeof(std::coroutine_handle<>));

inline constexpr auto to_string(const TaskPromiseWaiting value) {
  if (value.IsDetached()) {
    return std::format("{}", value.GetDetachedState());
  }
  return std::format(
    "{:#018x}",
    reinterpret_cast<uintptr_t>(value.GetConsumerHandle().address()));
}

inline constexpr TaskPromiseWaiting TaskPromiseProducerPending {
  DetachedState::ProducerPending};
inline constexpr TaskPromiseWaiting TaskPromiseConsumerDestroyed {
  DetachedState::ConsumerDestroyed};
inline constexpr TaskPromiseWaiting TaskPromiseProducerCompleted {
  DetachedState::ProducerCompleted};
inline constexpr TaskPromiseWaiting TaskPromiseProducerCompleting {
  DetachedState::ProducerCompleting};
inline constexpr TaskPromiseWaiting TaskPromiseProducerResumingConsumer {
  DetachedState::ProducerResumingConsumer};
inline constexpr TaskPromiseWaiting TaskPromiseConsumerResumed {
  DetachedState::ConsumerResumed};
inline constexpr TaskPromiseWaiting TaskPromiseReadyWithoutSuspend {
  DetachedState::ReadyWithoutSuspend};

template <class TTraits>
struct TaskPromiseBase;

template <class TTraits>
struct TaskPromise;

enum class TaskExceptionBehavior {
  StoreAndRethrow,
  Terminate,
};

enum class TaskAwaiting {
  Required,
  Optional,
  NotSupported,
};

enum class TaskCompletionThread {
  OriginalThread,
  AnyThread,
};

template <class TResult>
struct TaskTraits {
  static constexpr auto OnException = TaskExceptionBehavior::StoreAndRethrow;
  static constexpr auto Awaiting = TaskAwaiting::Required;
  static constexpr auto CompletionThread = TaskCompletionThread::OriginalThread;

  using result_type = TResult;
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
  TaskFinalAwaiter(TaskPromiseBase<TTraits>& promise) : mPromise(promise) {
    if constexpr (!DebugTaskCoroutines) {
      return;
    }

    using enum TaskPromiseResultState;
    switch (const auto state = mPromise.mResultState.Get()) {
      case HaveException:
      case HaveResult:
      case HaveVoidResult:
        return;
      default:
        mPromise.mContext.fatal("Invalid state for final awaiter: {:#}", state);
    }
  }

  bool await_ready() const noexcept { return false; }

  template <class TPromise>
  void await_suspend(std::coroutine_handle<TPromise>) noexcept {
    auto oldState = mPromise.mWaiting.exchange(
      TaskPromiseProducerCompleting, std::memory_order_acq_rel);
    auto& context = mPromise.mContext;
    if (oldState.IsDetached()) {
      using enum DetachedState;
      switch (oldState.GetDetachedState()) {
        case ProducerPending: {
          auto previous = TaskPromiseProducerCompleting;
          if (mPromise.mWaiting.compare_exchange_strong(
                previous,
                TaskPromiseProducerCompleted,
                std::memory_order_acq_rel)) {
            return;
          }
          if (previous == TaskPromiseConsumerDestroyed) {
            OPENKNEEBOARD_ASSERT(
              previous.GetOwner() == TaskPromiseWaiting::Owner::Producer);
            mPromise.destroy();
            return;
          }
          if (previous == TaskPromiseReadyWithoutSuspend) {
            return;
          }
          OPENKNEEBOARD_ASSERT(previous.ContainsConsumerHandle());
          oldState = previous;
          if (!mPromise.mWaiting.compare_exchange_strong(
                previous,
                TaskPromiseProducerCompleting,
                std::memory_order_acq_rel)) {
            if (previous.GetOwner() == TaskPromiseWaiting::Owner::Producer)
              [[likely]] {
              mPromise.destroy();
              return;
            }
            if (previous != oldState) {
              context.fatal("Task double-awaited");
            }
            context.fatal(
              "non-producer task state change after await: coro handle -> {}",
              magic_enum::enum_name(previous.GetDetachedState()));
          }
          break;
        }
        // completed twice
        case ProducerCompleting:
        case ProducerCompleted:
        case ProducerResumingConsumer:
        // must happen *after* completion
        case ConsumerResumed:
        case ReadyWithoutSuspend:
          context.fatal(
            "Invalid state in TaskFinalAwaiter::await_suspend: {}",
            magic_enum::enum_name(oldState.GetDetachedState()));
        case ConsumerDestroyed:
          OPENKNEEBOARD_ASSERT(
            oldState.GetOwner() == TaskPromiseWaiting::Owner::Producer);
          mPromise.destroy();
          return;
      }
    }

    OPENKNEEBOARD_ASSERT(oldState.ContainsConsumerHandle());
    if (auto expected = TaskPromiseProducerCompleting;
        !mPromise.mWaiting.compare_exchange_strong(
          expected,
          TaskPromiseProducerResumingConsumer,
          std::memory_order_acq_rel)) [[unlikely]] {
      if (expected.ContainsConsumerHandle()) {
        context.fatal("Double-resume in TaskFinalAwaiter::await_suspend");
      }
      context.fatal(
        "Unexpected state change (coro -> {}) in "
        "TaskFinalAwaiter::await_suspend",
        magic_enum::enum_name(expected.GetDetachedState()));
    }

    if constexpr (
      TTraits::CompletionThread == TaskCompletionThread::AnyThread) {
      mPromise.resumed();
      oldState.GetConsumerHandle().resume();
    } else {
      if (context.mThreadID == std::this_thread::get_id()) {
        mPromise.resumed();
        oldState.GetConsumerHandle().resume();
        return;
      }
      bounce_to_thread_pool(oldState.GetConsumerHandle());
    }
  }

  void await_resume() const noexcept {}

 private:
  struct ResumeData {
    TaskPromiseBase<TTraits>& mPromise;
    void* mConsumer {};
  };

  void bounce_to_thread_pool(const std::coroutine_handle<> handle) {
    const auto& context = mPromise.mContext;

    auto resumeData = new ResumeData {
      .mPromise = mPromise,
      .mConsumer = handle.address(),
    };
    OPENKNEEBOARD_ASSERT(
      resumeData->mConsumer != std::noop_coroutine().address());
    const auto threadPoolSuccess = TrySubmitThreadpoolCallback(
      &TaskFinalAwaiter<TTraits>::thread_pool_callback, resumeData, nullptr);

    if (threadPoolSuccess) [[likely]] {
      return;
    }
    delete resumeData;
    const auto threadPoolError = GetLastError();
    context.fatal(
      "Failed to enqueue resumption on thread pool: {:010x}",
      static_cast<uint32_t>(threadPoolError));
    // Above is no return, so we don't need to return noop_coroutine()
  }

  static void thread_pool_callback(
    PTP_CALLBACK_INSTANCE,
    void* userData) noexcept {
    const std::unique_ptr<ResumeData> resumeData {
      static_cast<ResumeData*>(userData)};
    ComCallData comData {.pUserDefined = userData};
    const auto result =
      resumeData->mPromise.mContext.mCOMCallback->ContextCallback(
        &TaskFinalAwaiter<TTraits>::target_thread_callback,
        &comData,
        IID_ICallbackWithNoReentrancyToApplicationSTA,
        5,
        NULL);
    if (SUCCEEDED(result)) [[likely]] {
      return;
    }
    resumeData->mPromise.mContext.fatal(
      "Failed to enqueue coroutine resumption for the desired thread: "
      "{:#010x}",
      static_cast<uint32_t>(result));
  }

  static HRESULT target_thread_callback(ComCallData* comData) noexcept {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskFinalAwaiter<>::target_thread_callback()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(comData, "ComData"));

    const auto resumeData = *static_cast<ResumeData*>(comData->pUserDefined);
    const auto context = resumeData.mPromise.mContext;

    TraceLoggingWrite(
      gTraceProvider,
      "TaskFinalAwaiter<>::target_thread_callback()/ResumeCoro",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(comData, "ComData"),
      TraceLoggingCodePointer(context.mCaller.mValue, "Caller"));

    try {
      resumeData.mPromise.resumed();
      std::coroutine_handle<>::from_address(resumeData.mConsumer).resume();
    } catch (...) {
      dprint.Warning("std::coroutine_handle<>::resume() threw an exception");
      fatal_with_exception(std::current_exception());
    }

    TraceLoggingWrite(
      gTraceProvider,
      "TaskFinalAwaiter<>::target_thread_callback()/CoroComplete",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(comData, "ComData"),
      TraceLoggingCodePointer(context.mCaller.mValue, "Caller"));
    return S_OK;
  }
};

template <class TTraits>
struct Task;

template <class TTraits>
struct TaskPromiseBase {
  using traits_type = TTraits;
  using enum TaskPromiseResultState;
  AtomicStateMachine<
    TaskPromiseResultState,
    NoResult,
    std::array {
      Transition {NoResult, HaveResult},
      Transition {HaveResult, ReturnedResult},

      Transition {NoResult, HaveException},
      Transition {HaveException, ThrownException},

      Transition {NoResult, HaveVoidResult},
      Transition {HaveVoidResult, ReturnedVoid},
    },
    std::nullopt>
    mResultState;

  StackTrace mUncaughtStack;
  std::exception_ptr mUncaught {};

  std::atomic<TaskPromiseWaiting> mWaiting {TaskPromiseProducerPending};

  TaskContext mContext;
  const std::coroutine_handle<> mProducerHandle;
  TaskExceptionBehavior mOnException = TTraits::OnException;

  TaskPromiseBase() = delete;
  TaskPromiseBase(const TaskPromiseBase<TTraits>&) = delete;
  TaskPromiseBase<TTraits>& operator=(const TaskPromiseBase<TTraits>&) = delete;

  TaskPromiseBase(TaskPromiseBase<TTraits>&&) = delete;
  TaskPromiseBase<TTraits>& operator=(TaskPromiseBase<TTraits>&&) = delete;

  TaskPromiseBase(
    TaskContext&& context,
    const std::coroutine_handle<> self) noexcept
    : mContext(std::move(context)),
      mProducerHandle(self) {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskPromiseBase<>::TaskPromiseBase()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Promise"),
      TraceLoggingCodePointer(mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(mWaiting.load(std::memory_order_relaxed)).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mResultState.Get(std::memory_order_relaxed)).c_str(),
        "ResultState"));
  }

  auto get_return_object() { return static_cast<TaskPromise<TTraits>*>(this); }

  void unhandled_exception() noexcept {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskPromiseBase<>::unhandled_exception()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Promise"),
      TraceLoggingCodePointer(mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(mWaiting.load(std::memory_order_relaxed)).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mResultState.Get(std::memory_order_relaxed)).c_str(),
        "ResultState"));
    // Might not actually be likely, but let's optimize the path that doesn't
    // result in immediate program termination, as we don't really care about
    // performance at that point :)
    if (mOnException == TaskExceptionBehavior::StoreAndRethrow) [[likely]] {
      mUncaught = std::current_exception();
      mUncaughtStack = StackTrace::GetForMostRecentException();
      mResultState.Transition<NoResult, HaveException>();
    } else {
      OPENKNEEBOARD_ASSERT(mOnException == TaskExceptionBehavior::Terminate);
      fatal_with_exception(std::current_exception());
    }
  }

  void resumed() {
    const auto oldState =
      mWaiting.exchange(TaskPromiseConsumerResumed, std::memory_order_acq_rel);
    if (oldState.ContainsConsumerHandle()) [[unlikely]] {
      mContext.fatal(
        "Have a handle in TaskPromiseBase::resume(), expected Destroyed or "
        "Completing");
      return;
    }

    if (oldState.GetOwner() == TaskPromiseWaiting::Owner::Producer) {
      destroy();
    }
  }

  void destroy() {
    // Kept as a method in case we want tracing or similar later
    mProducerHandle.destroy();
  }

  void consumer_destroyed() {
    const auto oldState = mWaiting.exchange(
      TaskPromiseConsumerDestroyed, std::memory_order_acq_rel);
    OPENKNEEBOARD_ASSERT(
      oldState.IsDetached(), "task destroyed while being awaited");
    if constexpr (TTraits::Awaiting == TaskAwaiting::Required) {
      if (
        oldState != TaskPromiseConsumerResumed
        && oldState != TaskPromiseReadyWithoutSuspend) [[unlikely]] {
        mContext.fatal(
          "task must be moved or awaited - destroyed in state `{}`",
          magic_enum::enum_name(oldState.GetDetachedState()));
      }
    }
    if (oldState.GetOwner() == TaskPromiseWaiting::Owner::Consumer) {
      destroy();
    }
  }

  [[nodiscard]]
  bool is_ready_without_suspend() {
    auto expected = TaskPromiseProducerCompleted;
    return mWaiting.compare_exchange_strong(
      expected, TaskPromiseReadyWithoutSuspend, std::memory_order_acq_rel);
  }

  std::suspend_never initial_suspend() noexcept {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskPromiseBase<>::initial_suspend()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Promise"),
      TraceLoggingCodePointer(mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(mWaiting.load(std::memory_order_relaxed)).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mResultState.Get(std::memory_order_relaxed)).c_str(),
        "ResultState"));
    return {};
  };

  TaskFinalAwaiter<TTraits> final_suspend() noexcept {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskPromiseBase<>::final_suspend()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Promise"),
      TraceLoggingCodePointer(mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(mWaiting.load(std::memory_order_relaxed)).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mResultState.Get(std::memory_order_relaxed)).c_str(),
        "ResultState"),
      TraceLoggingValue(std::uncaught_exceptions(), "UncaughtExceptions"));
    return {*this};
  }

  template <class TAwaitable>
  TAwaitable&& await_transform(TAwaitable&& it) noexcept {
    return static_cast<TAwaitable&&>(it);
  }

  auto await_transform(noexcept_task_t) noexcept {
    mOnException = TaskExceptionBehavior::Terminate;
    return std::suspend_never {};
  }
};

template <class TTraits>
struct TaskPromise : TaskPromiseBase<TTraits> {
  TaskPromise(std::optional<StackFramePointer> caller = std::nullopt)
    : TaskPromiseBase<TTraits>(
        caller.value_or(StackFramePointer::caller(2)),
        std::coroutine_handle<TaskPromise<TTraits>>::from_promise(*this)) {}

  typename TTraits::result_type mResult;

  void return_value(typename TTraits::result_type&& result) noexcept {
    // clang-cl dislikes 'this' in TraceLoggingValue
    const auto& self = *this;
    TraceLoggingWrite(
      gTraceProvider,
      "TaskPromise<>::return_value()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Promise"),
      TraceLoggingCodePointer(self.mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(self.mWaiting.load(std::memory_order_relaxed)).c_str(),
        "Waiting"),
      TraceLoggingValue(
        std::format("{}", self.mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
    using enum TaskPromiseResultState;
    mResult = std::move(result);
    this->mResultState.template Transition<NoResult, HaveResult>();
  }
};

template <class TTraits>
  requires std::same_as<typename TTraits::result_type, void>
struct TaskPromise<TTraits> : TaskPromiseBase<TTraits> {
  TaskPromise(std::optional<StackFramePointer> caller = std::nullopt)
    : TaskPromiseBase<TTraits>(
        caller.value_or(StackFramePointer::caller(3)),
        std::coroutine_handle<TaskPromise<TTraits>>::from_promise(*this)) {}

  void return_void() noexcept {
    // clang-cl dislikes 'this' in TraceLoggingValue
    const auto& self = *this;
    TraceLoggingWrite(
      gTraceProvider,
      "TaskPromise<void>::return_void()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Promise"),
      TraceLoggingCodePointer(this->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(self.mWaiting.load(std::memory_order_relaxed)).c_str(),
        "Waiting"),
      TraceLoggingValue(
        std::format("{}", self.mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
    using enum TaskPromiseResultState;
    this->mResultState.template Transition<NoResult, HaveVoidResult>();
  }
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

    TraceLoggingWrite(
      gTraceProvider,
      "TaskAwaiter<>::TaskAwaiter()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(&mPromise, "Promise"),
      TraceLoggingCodePointer(mPromise->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(mPromise->mWaiting.load(std::memory_order_relaxed)).c_str(),
        "Waiting"),
      TraceLoggingValue(
        std::format("{}", mPromise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
  }

  [[nodiscard]]
  bool await_ready() const noexcept {
    return mPromise->is_ready_without_suspend();
  }

  void await_suspend(std::coroutine_handle<> caller) {
    auto previous = TaskPromiseProducerPending;
    if (mPromise->mWaiting.compare_exchange_strong(
          previous, caller, std::memory_order_acq_rel)) {
      return;
    }

    if (!previous.IsDetached()) [[unlikely]] {
      mPromise->mContext.fatal("tasks can only be awaited once");
    }

    if (previous == TaskPromiseProducerCompleting) {
      if (mPromise->mWaiting.compare_exchange_strong(
            previous, caller, std::memory_order_acq_rel)) {
        return;
      }
    }

    if (previous == TaskPromiseProducerCompleted) {
      if (mPromise->mWaiting.compare_exchange_strong(
            previous,
            TaskPromiseReadyWithoutSuspend,
            std::memory_order_acq_rel)) {
        mPromise->resumed();
        caller.resume();
        return;
      }
    }

    mPromise->mContext.fatal(
      "Invalid state `{}` in TaskAwaiter::await_suspend",
      magic_enum::enum_name(previous.GetDetachedState()));
    __assume(0);
  }

  auto await_resume() {
    OPENKNEEBOARD_ASSERT(
      mPromise, "can't resume a moved or already-resumed task");

    const auto waiting = mPromise->mWaiting.load(std::memory_order_acquire);
    TraceLoggingWrite(
      gTraceProvider,
      "TaskAwaiter<>::await_resume()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(&mPromise, "Promise"),
      TraceLoggingCodePointer(mPromise->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(to_string(waiting).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mPromise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
    OPENKNEEBOARD_ASSERT(
      waiting == TaskPromiseConsumerResumed
      || waiting == TaskPromiseReadyWithoutSuspend);

    OPENKNEEBOARD_ASSERT(
      waiting.GetOwner() == TaskPromiseWaiting::Owner::Consumer);
    const auto cleanup =
      scope_exit([pp = &mPromise] { std::exchange(*pp, nullptr)->destroy(); });

    using enum TaskPromiseResultState;
    if (mPromise->mUncaught) {
      mPromise->mResultState
        .template Transition<HaveException, ThrownException>();
      StackTrace::SetForNextException(std::move(mPromise->mUncaughtStack));
      std::rethrow_exception(std::move(mPromise->mUncaught));
    }

    if constexpr (std::same_as<typename TTraits::result_type, void>) {
      mPromise->mResultState
        .template Transition<HaveVoidResult, ReturnedVoid>();
      return;
    } else {
      mPromise->mResultState.template Transition<HaveResult, ReturnedResult>();
      return std::move(mPromise->mResult);
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

  static_assert(TTraits::Awaiting != TaskAwaiting::Optional);

  [[nodiscard]]
  constexpr bool is_moved() const noexcept {
    return !mPromise;
  }

  Task(Task&& other) noexcept {
    mPromise = std::exchange(other.mPromise, nullptr);
    TraceLoggingWrite(
      gTraceProvider,
      "Task<>::Task(Task&&)",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Task"),
      TraceLoggingPointer(mPromise, "Promise"));
  }

  Task& operator=(Task&& other) noexcept {
    mPromise = std::exchange(other.mPromise, nullptr);
    TraceLoggingWrite(
      gTraceProvider,
      "Task<>::operator=(Task&&)",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Task"),
      TraceLoggingPointer(mPromise, "Promise"));
    return *this;
  }

  Task(std::nullptr_t) = delete;
  Task(promise_t* promise) : mPromise(promise) {
    TraceLoggingWrite(
      gTraceProvider,
      "Task<>::Task(promise_t*)",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Task"),
      TraceLoggingPointer(promise, "Promise"),
      TraceLoggingCodePointer(promise->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(to_string(promise->mWaiting).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", promise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));

    OPENKNEEBOARD_ASSERT(promise);
  }

  ~Task() {
    TraceLoggingWrite(
      gTraceProvider,
      "Task<>::~Task()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Task"),
      TraceLoggingPointer(mPromise, "Promise"));
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
    TraceLoggingWrite(
      gTraceProvider,
      "Task<>::operator co_await()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Task"),
      TraceLoggingPointer(mPromise, "Promise"));

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
 * - statically requires that the reuslt is discarded via [[nodiscard]]
 * - calls fatal() if there is an uncaught exception
 * - propagates uncaught exceptions back to the caller
 *
 * This is similar to wil::com_task(), except that it doesn't have as many
 * dependencies/unwanted interaections with various parts of Windows.h and
 * ole2.h
 *
 * If you want to store one, use an `std::optional<task<T>>`; to co_await the
 * stored task, use `co_await std::move(optional).value()`
 */
template <class T>
using task = detail::Task<detail::TaskTraits<T>>;

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

namespace OpenKneeboard {

struct fire_and_forget : detail::Task<detail::FireAndForgetTraits> {
  using detail::Task<detail::FireAndForgetTraits>::Task;

  static fire_and_forget wrap(auto toWrap, auto... args) {
    co_await std::invoke(std::move(toWrap), std::move(args)...);
    co_return;
  }
};

}// namespace OpenKneeboard
