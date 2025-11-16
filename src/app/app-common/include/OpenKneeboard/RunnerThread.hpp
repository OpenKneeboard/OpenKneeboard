
// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
#include <OpenKneeboard/ThreadGuard.hpp>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Dispatching.h>

#include <OpenKneeboard/task.hpp>

#include <functional>
#include <stop_token>
#include <string_view>

namespace OpenKneeboard {

class RunnerThread final {
 public:
  RunnerThread();
  RunnerThread(
    std::string_view name,
    std::function<task<void>(std::stop_token)> impl,
    const std::source_location& loc = std::source_location::current());
  ~RunnerThread();

  inline operator bool() const {
    return !!mDQC;
  }

  RunnerThread(const RunnerThread&) = delete;
  RunnerThread& operator=(const RunnerThread&) = delete;

  RunnerThread(RunnerThread&&) = delete;
  RunnerThread& operator=(RunnerThread&&) noexcept;

 private:
  ThreadGuard mThreadGuard;
  std::stop_source mStopSource;
  std::string mName;
  winrt::handle mCompletionEvent;
  DispatcherQueueController mDQC {nullptr};

  void Stop();
};

}// namespace OpenKneeboard