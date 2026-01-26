// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "task_context.hpp"

namespace OpenKneeboard::detail {

struct TaskContextAwaiter {
  task_context& mContext;
  bool await_ready() const noexcept {
    return std::this_thread::get_id() == mContext.mThreadID;
  }

  void await_resume() const noexcept {}

  void await_suspend(const std::coroutine_handle<> caller) const noexcept {
    bounce_to_thread_pool(mContext, caller);
  }

 private:
  struct ThreadPoolData {
    task_context mContext {};
    std::coroutine_handle<> mConsumerHandle {};
  };

  static void bounce_to_thread_pool(
    task_context context,
    std::coroutine_handle<> consumer) noexcept {
    auto data = new ThreadPoolData {
      .mContext = std::move(context),
      .mConsumerHandle = consumer,
    };
    const auto threadPoolSuccess =
      TrySubmitThreadpoolCallback(&thread_pool_callback, data, nullptr);

    if (threadPoolSuccess) [[likely]] {
      return;
    }
    delete data;
    fatal(
      "Failed to enqueue resumption on thread pool: {:010x}",
      static_cast<uint32_t>(GetLastError()));
  }

  static void thread_pool_callback(
    PTP_CALLBACK_INSTANCE,
    void* userData) noexcept {
    std::unique_ptr<ThreadPoolData> data {
      static_cast<ThreadPoolData*>(userData)};

    ComCallData comData {.pUserDefined = data.get()};
    const auto result = data->mContext.mCOMCallback->ContextCallback(
      &target_thread_callback,
      &comData,
      IID_ICallbackWithNoReentrancyToApplicationSTA,
      5,
      nullptr);
    if (SUCCEEDED(result)) [[likely]] {
      return;
    }
    fatal(
      "Failed to enqueue coroutine resumption for the desired thread: "
      "{:#010x}",
      static_cast<uint32_t>(result));
  }

  static HRESULT target_thread_callback(ComCallData* const comData) noexcept {
    const auto data = static_cast<ThreadPoolData*>(comData->pUserDefined);
    OPENKNEEBOARD_ASSERT(data);
    OPENKNEEBOARD_ASSERT(data->mConsumerHandle);

    try {
      OPENKNEEBOARD_ASSERT(
        std::this_thread::get_id() == data->mContext.mThreadID);
      const auto consumer = std::exchange(data->mConsumerHandle, {});
      consumer.resume();
      return S_OK;
    } catch (...) {
      dprint.Warning("std::coroutine_handle<>::resume() threw an exception");
      fatal_with_exception(std::current_exception());
    }
  }
};

}// namespace OpenKneeboard::detail
