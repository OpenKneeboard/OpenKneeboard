// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <OpenKneeboard/task.hpp>

namespace OpenKneeboard::inline task_ns {

/** Resume on a previously saved context.
 *
 * Usage:
 *
 * auto context  = this_thread::get_task_context();
 * ... other things, including thread switching ...
 * co_await resume_in_context(context);
 */
inline auto resume_in_context(task_context& context) {
  return detail::TaskContextAwaiter(context);
}

}// namespace OpenKneeboard::inline task_ns
