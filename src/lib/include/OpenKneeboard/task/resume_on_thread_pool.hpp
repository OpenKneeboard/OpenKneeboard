// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <OpenKneeboard/fatal.hpp>

#include <coroutine>

#include <threadpoolapiset.h>

namespace OpenKneeboard::inline task_ns {

/// Awaitable that resumes the coroutine on a Windows Thread Pool worker thread.
[[nodiscard]]
inline auto resume_on_thread_pool() noexcept {
  struct threadpool_awaiter {
    bool await_ready() const noexcept { return false; }
    void await_resume() const noexcept {}

    void await_suspend(std::coroutine_handle<> handle) const noexcept {
      const auto success = TrySubmitThreadpoolCallback(
        [](PTP_CALLBACK_INSTANCE, void* context) {
          const auto handle = std::coroutine_handle<>::from_address(context);
          handle.resume();
        },
        handle.address(),
        nullptr);

      if (!success) [[unlikely]] {
        fatal(
          "Failed to enqueue resumption on thread pool: {:010x}",
          static_cast<uint32_t>(GetLastError()));
      }
    }
  };
  return threadpool_awaiter {};
}
}// namespace OpenKneeboard::inline task_ns
