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

#include <OpenKneeboard/ProcessShutdownBlock.h>
#include <OpenKneeboard/RunnerThread.h>

#include <source_location>

namespace OpenKneeboard {
RunnerThread::RunnerThread() = default;

RunnerThread::RunnerThread(
  std::string_view name,
  std::function<winrt::Windows::Foundation::IAsyncAction(std::stop_token)> impl,
  const std::source_location& loc) {
  mDQC = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::
    CreateOnDedicatedThread();

  mDQC.DispatcherQueue().TryEnqueue(
    [name = winrt::to_hstring(name),
     impl,
     stop = mStopSource.get_token(),
     loc]() -> winrt::fire_and_forget {
      ProcessShutdownBlock block(loc);
      SetThreadDescription(GetCurrentThread(), name.c_str());
      co_await impl(stop);
    });

  auto x = !!mDQC;
}

RunnerThread::~RunnerThread() {
  this->Stop();
}

void RunnerThread::Stop() {
  mThreadGuard.CheckThread();
  if (!mDQC) {
    return;
  }

  mStopSource.request_stop();

  ([](auto dqc) -> winrt::fire_and_forget {
    co_await dqc.ShutdownQueueAsync();
  })(std::move(mDQC));
  mDQC = {nullptr};
  mStopSource = {};
}

RunnerThread& RunnerThread::operator=(RunnerThread&& other) {
  this->Stop();
  mDQC = std::move(other.mDQC);
  mStopSource = std::move(other.mStopSource);

  other.mDQC = {nullptr};
  other.mStopSource = {};

  return *this;
}

}// namespace OpenKneeboard