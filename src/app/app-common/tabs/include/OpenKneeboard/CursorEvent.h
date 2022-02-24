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

#include <cstdint>

namespace OpenKneeboard {
enum class CursorPositionState {
  IN_CONTENT_RECT,
  NOT_IN_CONTENT_RECT,
};
enum class CursorTouchState {
  TOUCHING_SURFACE,
  NEAR_SURFACE,
  NOT_NEAR_SURFACE,
};
enum class CursorSource {
  WINDOW_POINTER,
  OTHER
};

struct CursorEvent {
  CursorSource mSource = CursorSource::OTHER;
  CursorPositionState mPositionState = CursorPositionState::NOT_IN_CONTENT_RECT;
  CursorTouchState mTouchState = CursorTouchState::NOT_NEAR_SURFACE;
  float mX = 0, mY = 0, mPressure = 0;
  uint32_t mButtons = 0;
};

}// namespace OpenKneeboard
