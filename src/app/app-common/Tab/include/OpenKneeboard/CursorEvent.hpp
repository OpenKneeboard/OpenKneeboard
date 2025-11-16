// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <cstdint>

namespace OpenKneeboard {

enum class CursorTouchState {
  TouchingSurface,
  NearSurface,
  NotNearSurface,
};

enum class CursorSource {
  WindowPointer,
  Tablet,
};

struct CursorEvent {
  CursorSource mSource = CursorSource::Tablet;
  CursorTouchState mTouchState = CursorTouchState::NotNearSurface;
  float mX = 0, mY = 0, mPressure = 0;
  uint32_t mButtons = 0;
};

}// namespace OpenKneeboard
