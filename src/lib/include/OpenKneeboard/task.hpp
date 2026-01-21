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

enum class TaskPromiseState : uintptr_t {
  Running = 1,
  Abandoned = 2,
  Completed = 3,
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

/* Function as a union so we can do std::atomic.
 *
 * - TaskPromiseState is sizeof(ptr), but always an invalid pointer
 * - coroutine handles actually contain a pointer
 */
struct TaskPromiseWaiting {
  // `nullptr` is considered to be a handle
  static_assert(!magic_enum::enum_contains<TaskPromiseState>(0));

  constexpr TaskPromiseWaiting() = default;
  constexpr TaskPromiseWaiting(const TaskPromiseState state)
    : mStorage {std::to_underlying(state)} {
  }

  constexpr TaskPromiseWaiting(std::coroutine_handle<> handle)
    : mStorage(std::bit_cast<uintptr_t>(handle.address())) {
  }

  constexpr bool ContainsState() const {
    return magic_enum::enum_contains<TaskPromiseState>(mStorage);
  }

  constexpr bool ContainsHandle() const {
    return !ContainsState();
  }

  constexpr TaskPromiseState GetState() const {
    OPENKNEEBOARD_ASSERT(ContainsState());
    return static_cast<TaskPromiseState>(mStorage);
  }

  constexpr std::coroutine_handle<> GetHandle() const {
    OPENKNEEBOARD_ASSERT(ContainsHandle());
    return std::coroutine_handle<>::from_address(
      std::bit_cast<void*>(mStorage));
  }

  constexpr bool operator==(const TaskPromiseWaiting&) const = default;

 private:
  uintptr_t mStorage {std::to_underlying(TaskPromiseState::Running)};
};
static_assert(
  std::is_trivially_copyable_v<TaskPromiseWaiting>,
  "TaskPromiseWaiting must be trivially copyable for use with std::atomic");
static_assert(sizeof(TaskPromiseWaiting) == sizeof(TaskPromiseState));
static_assert(sizeof(TaskPromiseWaiting) == sizeof(std::coroutine_handle<>));

inline constexpr auto to_string(const TaskPromiseWaiting value) {
  if (value.ContainsState()) {
    return std::format("{}", value.GetState());
  }
  return std::format(
    "{:#018x}", reinterpret_cast<uintptr_t>(value.GetHandle().address()));
}

inline constexpr TaskPromiseWaiting TaskPromiseRunning {
  TaskPromiseState::Running};
inline constexpr TaskPromiseWaiting TaskPromiseAbandoned {
  TaskPromiseState::Abandoned};
inline constexpr TaskPromiseWaiting TaskPromiseCompleted {
  TaskPromiseState::Completed};

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
    const auto state = mPromise.mResultState.Get();
    switch (state) {
      case HaveException:
      case HaveResult:
      case HaveVoidResult:
        return;
      default:
        mPromise.mContext.fatal("Invalid state for final awaiter: {:#}", state);
    }
  }

  bool await_ready() const noexcept {
    return false;
  }

  template <class TPromise>
  void await_suspend(std::coroutine_handle<TPromise>) noexcept {
    auto oldState = mPromise.mWaiting.exchange(
      TaskPromiseCompleted, std::memory_order_acq_rel);
    if (oldState == TaskPromiseAbandoned) {
      mPromise.destroy();
      return;
    }
    if (oldState == TaskPromiseRunning) {
      return;
    }

    // Must have a valid pointer, or we have corruption
    OPENKNEEBOARD_ASSERT(oldState != TaskPromiseCompleted);

    if constexpr (
      TTraits::CompletionThread == TaskCompletionThread::AnyThread) {
      oldState.GetHandle().resume();
    } else {
      resume_on_required_thread(oldState);
    }
  }

  void await_resume() const noexcept {
  }

 private:
  struct ResumeData {
    TaskContext mContext;
    std::coroutine_handle<> mCoro;
  };

  void resume_on_required_thread(const TaskPromiseWaiting oldState) {
    const auto& context = mPromise.mContext;
    if (context.mThreadID == std::this_thread::get_id()) {
      oldState.GetHandle().resume();
      return;
    }

    auto resumeData = new ResumeData {
      .mContext = context,
      .mCoro = oldState.GetHandle(),
    };
    const auto threadPoolSuccess = TrySubmitThreadpoolCallback(
      &TaskFinalAwaiter<TTraits>::resume_on_thread_pool, resumeData, nullptr);
    if (threadPoolSuccess) [[likely]] {
      return;
    }
    const auto threadPoolError = GetLastError();
    delete resumeData;
    context.fatal(
      "Failed to enqueue resumption on thread pool: {:010x}",
      static_cast<uint32_t>(threadPoolError));
  }

  static void resume_on_thread_pool(
    PTP_CALLBACK_INSTANCE,
    void* userData) noexcept {
    std::unique_ptr<ResumeData> resumeData {
      reinterpret_cast<ResumeData*>(userData)};

    ComCallData comData {.pUserDefined = resumeData.get()};
    const auto result = resumeData->mContext.mCOMCallback->ContextCallback(
      &TaskFinalAwaiter<TTraits>::resume_from_thread_pool,
      &comData,
      IID_ICallbackWithNoReentrancyToApplicationSTA,
      5,
      NULL);
    if (SUCCEEDED(result)) [[likely]] {
      return;
    }
    resumeData->mContext.fatal(
      "Failed to enqueue coroutine resumption for the desired thread: "
      "{:#010x}",
      static_cast<uint32_t>(result));
  }

  static HRESULT resume_from_thread_pool(ComCallData* comData) noexcept {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskFinalAwaiter<>::resume_from_thread_pool()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(comData, "ComData"));

    const auto& resumeData
      = *reinterpret_cast<ResumeData*>(comData->pUserDefined);
    const auto& context = resumeData.mContext;

    TraceLoggingWrite(
      gTraceProvider,
      "TaskFinalAwaiter<>::resume_from_thread_pool()/ResumeCoro",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(comData, "ComData"),
      TraceLoggingCodePointer(context.mCaller.mValue, "Caller"));

    try {
      resumeData.mCoro.resume();
    } catch (...) {
      dprint.Warning("std::coroutine_handle<>::resume() threw an exception");
      fatal_with_exception(std::current_exception());
    }

    TraceLoggingWrite(
      gTraceProvider,
      "TaskFinalAwaiter<>::resume_from_thread_pool()/CoroComplete",
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

  std::atomic<TaskPromiseWaiting> mWaiting {TaskPromiseRunning};

  TaskContext mContext;
  TaskExceptionBehavior mOnException = TTraits::OnException;

  TaskPromiseBase() = delete;
  TaskPromiseBase(const TaskPromiseBase<TTraits>&) = delete;
  TaskPromiseBase<TTraits>& operator=(const TaskPromiseBase<TTraits>&) = delete;

  TaskPromiseBase(TaskPromiseBase<TTraits>&&) = delete;
  TaskPromiseBase<TTraits>& operator=(TaskPromiseBase<TTraits>&&) = delete;

  TaskPromiseBase(TaskContext&& context) noexcept
    : mContext(std::move(context)) {
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

  auto get_return_object() {
    return static_cast<TaskPromise<TTraits>*>(this);
  }

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

  void abandon() {
    auto oldState
      = mWaiting.exchange(TaskPromiseAbandoned, std::memory_order_acq_rel);
    if (oldState == TaskPromiseRunning) {
      if constexpr (TTraits::Awaiting == TaskAwaiting::Required) {
        mContext.fatal("result *must* be awaited");
      }
      return;
    }
    this->destroy();
  }

  template <class Self>
  void destroy(this Self& self) {
    std::coroutine_handle<Self>::from_promise(self).destroy();
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
    : TaskPromiseBase<TTraits>(caller.value_or(StackFramePointer::caller(2))) {
  }

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
    this->mResultState.Transition<NoResult, HaveResult>();
  }
};

template <class TTraits>
  requires std::same_as<typename TTraits::result_type, void>
struct TaskPromise<TTraits> : TaskPromiseBase<TTraits> {
  TaskPromise(std::optional<StackFramePointer> caller = std::nullopt)
    : TaskPromiseBase<TTraits>(caller.value_or(StackFramePointer::caller(3))) {
  }

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

template <class T>
struct TaskPromiseDeleter {
  void operator()(T* promise) const noexcept {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskPromiseDeleter<>::operator()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingCodePointer(
        promise->mContext.mCaller.mValue, "PromiseCaller"),
      TraceLoggingPointer(promise, "Promise"),
      TraceLoggingValue(
        to_string(promise->mWaiting.load(std::memory_order_relaxed)).c_str(),
        "Waiting"),
      TraceLoggingValue(
        std::format("{}", promise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"),
      TraceLoggingCodePointer(_ReturnAddress(), "DeleterReturnAddress"),
      TraceLoggingCodePointer(
        _AddressOfReturnAddress(), "DeleterAddressOfReturnAddress"));

    promise->abandon();
  }
};

template <class T>
using TaskPromisePtr = std::unique_ptr<T, TaskPromiseDeleter<T>>;

template <class TTraits>
struct TaskAwaiter {
  using promise_t = TaskPromise<TTraits>;
  using promise_ptr_t = TaskPromisePtr<promise_t>;
  promise_ptr_t mPromise;

  TaskAwaiter() = delete;
  TaskAwaiter(const TaskAwaiter&) = delete;
  TaskAwaiter& operator=(const TaskAwaiter&) = delete;

  TaskAwaiter(TaskAwaiter&&) = delete;
  TaskAwaiter& operator=(TaskAwaiter&&) = delete;

  TaskAwaiter(promise_ptr_t&& init) : mPromise(std::move(init)) {
    TraceLoggingWrite(
      gTraceProvider,
      "TaskAwaiter<>::TaskAwaiter()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(mPromise.get(), "Promise"),
      TraceLoggingCodePointer(mPromise->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(
        to_string(mPromise->mWaiting.load(std::memory_order_relaxed)).c_str(),
        "Waiting"),
      TraceLoggingValue(
        std::format("{}", mPromise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
  }

  bool await_ready() const noexcept {
    const auto waiting = mPromise->mWaiting.load(std::memory_order_acquire);
    TraceLoggingWrite(
      gTraceProvider,
      "TaskAwaiter<>::await_ready()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(mPromise.get(), "Promise"),
      TraceLoggingCodePointer(mPromise->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(to_string(waiting).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mPromise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
    return waiting == TaskPromiseCompleted;
  }

  bool await_suspend(std::coroutine_handle<> caller) {
    auto oldState
      = mPromise->mWaiting.exchange(caller, std::memory_order_acq_rel);
    TraceLoggingWrite(
      gTraceProvider,
      "TaskAwaiter<>::await_suspend()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(mPromise.get(), "Promise"),
      TraceLoggingCodePointer(mPromise->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(to_string(oldState).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mPromise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
    return oldState == TaskPromiseRunning;
  }

  decltype(auto) await_resume() {
    const auto waiting = mPromise->mWaiting.load(std::memory_order_acquire);
    TraceLoggingWrite(
      gTraceProvider,
      "TaskAwaiter<>::await_resume()",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(mPromise.get(), "Promise"),
      TraceLoggingCodePointer(mPromise->mContext.mCaller.mValue, "Caller"),
      TraceLoggingValue(to_string(waiting).c_str(), "Waiting"),
      TraceLoggingValue(
        std::format("{}", mPromise->mResultState.Get(std::memory_order_relaxed))
          .c_str(),
        "ResultState"));
    OPENKNEEBOARD_ASSERT(waiting == TaskPromiseCompleted);

    using enum TaskPromiseResultState;
    if (mPromise->mUncaught) {
      mPromise->mResultState.Transition<HaveException, ThrownException>();
      StackTrace::SetForNextException(std::move(mPromise->mUncaughtStack));
      std::rethrow_exception(std::move(mPromise->mUncaught));
    }

    if constexpr (std::same_as<typename TTraits::result_type, void>) {
      mPromise->mResultState.Transition<HaveVoidResult, ReturnedVoid>();
      return;
    } else {
      mPromise->mResultState.Transition<HaveResult, ReturnedResult>();
      return std::move(mPromise->mResult);
    }
  }
};

template <class TTraits>
struct [[nodiscard]] Task {
  static constexpr bool must_await_v
    = TTraits::Awaiting == TaskAwaiting::Required;

  using promise_t = TaskPromise<TTraits>;
  using promise_ptr_t = TaskPromisePtr<promise_t>;

  using promise_type = promise_t;

  Task() = delete;
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  Task(Task&& other) {
    mPromise = std::move(other.mPromise);
    TraceLoggingWrite(
      gTraceProvider,
      "Task<>::Task(Task&&)",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Task"),
      TraceLoggingPointer(mPromise.get(), "Promise"));
  }

  Task& operator=(Task&& other) {
    mPromise = std::move(other.mPromise);
    TraceLoggingWrite(
      gTraceProvider,
      "Task<>::operator=(Task&&)",
      TraceLoggingKeyword(
        std::to_underlying(TraceLoggingEventKeywords::TaskCoro)),
      TraceLoggingPointer(this, "Task"),
      TraceLoggingPointer(mPromise.get(), "Promise"));
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
      TraceLoggingPointer(mPromise.get(), "Promise"));
    if constexpr (TTraits::Awaiting == TaskAwaiting::Required) {
      if (mPromise) [[unlikely]] {
        mPromise->mContext.fatal("all tasks must be either moved or awaited");
      }
    }
  }

  // just to give nice compiler error messages
  struct cannot_await_lvalue_use_std_move {
    void await_ready() {
    }
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
      TraceLoggingPointer(mPromise.get(), "Promise"));

    OPENKNEEBOARD_ASSERT(
      mPromise, "Can't await a task that has been moved or already awaited");
    return TaskAwaiter<TTraits> {std::move(mPromise)};
  }

  promise_ptr_t mPromise;
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
 * (and logged) about the exception. This allow allows the 'fatal-on-exception'
 * behavior to be considered an implementation detail of the function, instead
 * of part of the function signature.
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