// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "resume_on_signal.hpp"

#include <ranges>
#include <vector>

namespace OpenKneeboard::detail {

template <class T>
concept eager_coroutine = requires {
  typename std::coroutine_traits<T>::promise_type;
} && requires(typename std::coroutine_traits<T>::promise_type& p) {
  p.initial_suspend().await_ready();
  { p.initial_suspend().await_ready() } -> std::same_as<bool>;
  requires decltype(p.initial_suspend()) {}.await_ready();
};

}// namespace OpenKneeboard::detail

namespace OpenKneeboard::inline task_ns {

template <class T, class R>
  requires std::ranges::input_range<R>
  && std::same_as<std::ranges::range_value_t<R>, HANDLE>
task<std::expected<std::size_t, ResumeOnSignalError>>
resume_on_any_signal(R&& handles, const std::stop_token token, T&& timeout) {
  if (token.stop_requested()) {
    co_return std::unexpected {ResumeOnSignalError::Canceled};
  }

  std::stop_source stopper;
  const std::stop_callback callback(
    token, [&stopper] { stopper.request_stop(); });

  // We use `request_stop()` as an atomic gate so only the first
  // `resume_on_signal()` writes the result; however, if the external token is
  // triggered, `request_stop()` will fail for *all* the sub-coroutines. In all
  // other situations, the first will write, so we can just initialize it to
  // Canceled
  std::expected<std::size_t, ResumeOnSignalError> ret {
    std::unexpect, ResumeOnSignalError::Canceled};

  auto tasks = std::views::enumerate(std::forward<R>(handles))
    | std::views::transform(
                 std::bind_front(
                   [](
                     decltype(ret)& out,
                     std::stop_source myStopper,
                     const auto myTimeout,
                     const auto item) -> any_thread_task<void> {
                     const auto [i, handle] = item;
                     const auto result = co_await resume_on_signal(
                       handle, myStopper.get_token(), myTimeout);
                     if (!myStopper.request_stop()) {
                       co_return;
                     }
                     if (result) {
                       out = i;
                       co_return;
                     }
                     out = std::unexpected {result.error()};
                   },
                   std::ref(ret),
                   stopper,
                   std::forward<T>(timeout)))
    | std::ranges::to<std::vector>();

  if (tasks.empty()) {
    co_return std::unexpected {ResumeOnSignalError::InvalidArgument};
  }

  static_assert(
    detail::eager_coroutine<typename decltype(tasks)::value_type>,
    "for parallel execution, tasks must start before co_await");
  // otherwise we'll just wait for the first HANDLE

  for (auto&& task: tasks) {
    co_await std::move(task);
  }

  co_return ret;
}

template <class R>
auto resume_on_any_signal(R&& handles, const std::stop_token token) {
  return resume_on_any_signal(
    std::forward<R>(handles), token, std::chrono::milliseconds::zero());
}

template <class R>
task<void> resume_on_any_signal(R&& handles) {
  std::ignore = co_await resume_on_any_signal(
    std::forward<R>(handles), {}, std::chrono::milliseconds::zero());
}

}// namespace OpenKneeboard::inline task_ns
