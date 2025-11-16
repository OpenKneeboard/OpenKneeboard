// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <source_location>

#include <Windows.h>

#include <processthreadsapi.h>

namespace OpenKneeboard {

// Logs if destruction thread != creation thread
class ThreadGuard final {
 public:
  ThreadGuard(
    const std::source_location& loc = std::source_location::current());
  ~ThreadGuard();
  void CheckThread(
    const std::source_location& loc = std::source_location::current()) const;

 private:
  DWORD mThreadID;
  std::source_location mLocation;
};

}// namespace OpenKneeboard