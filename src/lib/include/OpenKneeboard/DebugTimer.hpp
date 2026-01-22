// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <chrono>
#include <string>

namespace OpenKneeboard {

class DebugTimer {
 public:
  DebugTimer(std::string_view label);
  ~DebugTimer();

  void End();

 private:
  std::string mLabel;
  std::chrono::steady_clock::time_point mStart;

  bool mFinished = false;
};

}// namespace OpenKneeboard
