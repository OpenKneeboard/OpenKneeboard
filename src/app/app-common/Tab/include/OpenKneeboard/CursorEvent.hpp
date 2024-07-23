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

#define OPENKNEEBOARD_CURSORTOUCHSTATE_VALUES \
  IT(TouchingSurface) \
  IT(NearSurface) \
  IT(NotNearSurface)

enum class CursorTouchState {
#define IT(x) x,
  OPENKNEEBOARD_CURSORTOUCHSTATE_VALUES
#undef IT
};

enum class CursorSource { WindowPointer, Tablet };

struct CursorEvent {
  CursorSource mSource = CursorSource::Tablet;
  CursorTouchState mTouchState = CursorTouchState::NotNearSurface;
  float mX = 0, mY = 0, mPressure = 0;
  uint32_t mButtons = 0;
};

}// namespace OpenKneeboard
