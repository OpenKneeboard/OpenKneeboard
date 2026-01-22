// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/DebugTimer.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <format>

namespace OpenKneeboard {

DebugTimer::DebugTimer(std::string_view label)
  : mLabel(label),
    mStart(std::chrono::steady_clock::now()) {}

DebugTimer::~DebugTimer() { this->End(); }

void DebugTimer::End() {
  if (mFinished) {
    return;
  }

  mFinished = true;
  dprint(
    "Timer: {} = {}",
    mLabel,
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - mStart));
}

}// namespace OpenKneeboard
