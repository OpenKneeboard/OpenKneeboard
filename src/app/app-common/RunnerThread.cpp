// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/RunnerThread.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <source_location>

namespace OpenKneeboard {
RunnerThread::RunnerThread() = default;

RunnerThread::RunnerThread(
  std::string_view name,
  std::function<task<void>(std::stop_token)> impl,
  // Not a coroutine.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
  const std::source_location& loc)
  : mName(std::string {name}) {
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
  mCompletionEvent
    = winrt::handle {CreateEventW(nullptr, FALSE, FALSE, nullptr)};

  mDQC.DispatcherQueue().TryEnqueue(std::bind_front(
    [](auto name, auto impl, auto stop, auto loc, auto event)
      -> OpenKneeboard::fire_and_forget {
      const scope_exit setEventOnExit([event, name]() {
        TraceLoggingWrite(
          gTraceProvider,
          "RunnerThread/SetEvent",
          TraceLoggingString(winrt::to_string(name).c_str(), "Name"));
        SetEvent(event);
      });
      ProcessShutdownBlock block(loc);
      SetThreadDescription(GetCurrentThread(), name.c_str());
      ThreadGuard threadGuard;
      co_await impl(stop);
    },
    winrt::to_hstring(name),
    impl,
    mStopSource.get_token(),
    loc,
    mCompletionEvent.get()));
}

RunnerThread::~RunnerThread() {
  OPENKNEEBOARD_TraceLoggingScope("RunnerThread::~RunnerThread()");
  this->Stop();
}

void RunnerThread::Stop() {
  mThreadGuard.CheckThread();
  if (!mDQC) {
    return;
  }

  TraceLoggingWrite(
    gTraceProvider,
    "RunnerThread::Stop()/request_stop()",
    TraceLoggingString(mName.c_str(), "Name"));
  mStopSource.request_stop();

  ([](auto event, auto dqc) -> OpenKneeboard::fire_and_forget {
    dprint("Waiting for completion event");
    co_await winrt::resume_on_signal(event.get());
    dprint("Shutting down runner thread DQC");
    co_await dqc.ShutdownQueueAsync();
    dprint("Runner thread DQC shut down");
  })(std::move(mCompletionEvent), std::move(mDQC));
}

RunnerThread& RunnerThread::operator=(RunnerThread&& other) noexcept {
  this->Stop();
  mDQC = std::move(other.mDQC);
  mStopSource = std::move(other.mStopSource);
  mCompletionEvent = std::move(other.mCompletionEvent);

  other.mDQC = {nullptr};
  other.mStopSource = {};

  return *this;
}

}// namespace OpenKneeboard