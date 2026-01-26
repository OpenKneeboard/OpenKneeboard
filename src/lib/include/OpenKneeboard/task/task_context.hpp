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

 protected:
  std::thread::id mThreadID = std::this_thread::get_id();
  wil::com_ptr<IContextCallback> mCOMCallback;

  task_context() {
    [[maybe_unused]] const auto hr =
      CoGetObjectContext(IID_PPV_ARGS(mCOMCallback.put()));
    OPENKNEEBOARD_ASSERT(
      SUCCEEDED(hr),
      "Attempted to create a task_context<> without a COM context: {:#010x}",
      static_cast<uint32_t>(hr));
  }
};

namespace this_thread {
inline task_context get_task_context() { return task_context {}; }
}// namespace this_thread

}// namespace OpenKneeboard::inline task_ns
