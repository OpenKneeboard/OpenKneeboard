
/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
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