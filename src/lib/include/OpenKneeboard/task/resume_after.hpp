// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "ThreadPoolAwaitable.hpp"

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/task.hpp>

#include <chrono>
#include <stop_token>

#include <threadpoolapiset.h>
#include <winnt.h>

namespace OpenKneeboard::detail {

enum class TimerAwaitableResult {
  Pending,
  Success,
  Canceled,
};

struct TimerAwaitable
  : ThreadPoolAwaitable<TimerAwaitable, TimerAwaitableResult> {
  static constexpr auto pending_result = TimerAwaitableResult::Pending;
  static constexpr auto canceled_result = TimerAwaitableResult::Canceled;

  template <class Rep, class Period = std::ratio<1>>
  TimerAwaitable(
    std::chrono::duration<Rep, Period> delay,
    const std::stop_token stopToken)
    : ThreadPoolAwaitable<TimerAwaitable, TimerAwaitableResult>(stopToken) {
    OPENKNEEBOARD_ASSERT(
      delay > decltype(delay)::zero(), "Sleep duration must be > 0");

    // negative filetime = interval, positive = absolute
    const auto count = static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::file_clock::duration>(delay)
        .count());
    ULARGE_INTEGER ulDuration {.QuadPart = std::bit_cast<uint64_t>(-count)};
    mTime = {
      .dwLowDateTime = ulDuration.LowPart,
      .dwHighDateTime = ulDuration.HighPart,
    };
  }

  void InitThreadPool() {
    OPENKNEEBOARD_ASSERT(!mTPTimer);
    mTPTimer =
      CreateThreadpoolTimer(&TimerAwaitable::ThreadPoolCallback, this, nullptr);
    SetThreadpoolTimer(mTPTimer, &mTime, 0, 0);
  }

  template <ThreadPoolAwaitableState From>
  void CleanupThreadPool() {
    if (!mTPTimer) {
      return;
    }
    if constexpr (From == ThreadPoolAwaitableState::Canceling) {
      this->CancelThreadPool();
    }
    CloseThreadpoolTimer(mTPTimer);
    mTPTimer = nullptr;
  }

  void CancelThreadPool() {
    OPENKNEEBOARD_ASSERT(mTPTimer);
    SetThreadpoolTimer(mTPTimer, nullptr, 0, 0);
    WaitForThreadpoolTimerCallbacks(mTPTimer, true);
  }

 private:
  FILETIME mTime {};
  PTP_TIMER mTPTimer {nullptr};

  static void ThreadPoolCallback(
    PTP_CALLBACK_INSTANCE const instance,
    void* context,
    PTP_TIMER) {
    auto& self = *reinterpret_cast<TimerAwaitable*>(context);
    self.OnResult<TimerAwaitableResult::Success>(instance);
  }
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

enum class TimerError {
  Canceled,
};

/** Alternative to `winrt::resume_after()` that can cancel without exceptions.
 *
 * The primary benefit is that this makes breaking on all exceptions practical
 * when debugging program shutdown; as well as the irrelevant breakpoints,
 * it can also effect timing, making intermittent issues harder to reproduce.
 */
task<std::expected<void, TimerError>> resume_after(
  auto duration,
  std::stop_token token) {
  using enum detail::TimerAwaitableResult;
  switch (co_await detail::TimerAwaitable {duration, token}) {
    case Pending:
      fatal("TimerAwaitable returned 'pending'");
    case Success:
      co_return {};
    case Canceled:
      co_return std::unexpected {TimerError::Canceled};
  }
}

task<void> resume_after(auto duration) {
  [[maybe_unused]] const auto timeout = co_await resume_after(duration, {});
  OPENKNEEBOARD_ASSERT(
    timeout,
    "Got a cancellation from an awaitable without a cancellation token");
  co_return;
}

}// namespace OpenKneeboard
