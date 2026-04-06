// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <OpenKneeboard/dprint.hpp>

#include <combaseapi.h>
#include <ctxtcall.h>

#include <cinttypes>
#include <thread>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
#include <wil/com.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace OpenKneeboard::detail {
struct TaskContext;
struct TaskContextAwaiter;
}// namespace OpenKneeboard::detail

namespace OpenKneeboard::inline task_ns {

struct task_context;
namespace this_thread {
task_context get_task_context();
}

struct task_context {
  friend struct detail::TaskContextAwaiter;

  friend task_context this_thread::get_task_context();

  [[nodiscard]]
  bool is_this_thread() const noexcept {
    return mThreadID == std::this_thread::get_id();
  }

  [[nodiscard]]
  bool is_same_com_context() const noexcept {
    ULONG_PTR current {};
    [[maybe_unused]] const auto hr = CoGetContextToken(&current);
    OPENKNEEBOARD_ASSERT(SUCCEEDED(hr));
    return current == mCOMContextToken;
  }

 protected:
  std::thread::id mThreadID = std::this_thread::get_id();
  ULONG_PTR mCOMContextToken {};
  wil::com_ptr<IContextCallback> mCOMCallback;

  task_context() {
    [[maybe_unused]] const auto hr =
      CoGetObjectContext(IID_PPV_ARGS(mCOMCallback.put()));
    OPENKNEEBOARD_ASSERT(
      SUCCEEDED(hr),
      "Attempted to create a task_context<> without a COM context: {:#010x}",
      static_cast<uint32_t>(hr));
    [[maybe_unused]] const auto tokenHr = CoGetContextToken(&mCOMContextToken);
    OPENKNEEBOARD_ASSERT(
      SUCCEEDED(tokenHr),
      "Failed to capture COM context token: {:#010x}",
      static_cast<uint32_t>(tokenHr));
  }
};

namespace this_thread {
inline task_context get_task_context() { return task_context {}; }
}// namespace this_thread

}// namespace OpenKneeboard::inline task_ns
