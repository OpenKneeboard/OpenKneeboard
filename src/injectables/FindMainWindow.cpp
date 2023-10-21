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
#include "FindMainWindow.h"

namespace OpenKneeboard {

static BOOL CALLBACK FindMainWindowCallback(HWND handle, LPARAM param) {
  DWORD windowProcess = 0;
  GetWindowThreadProcessId(handle, &windowProcess);
  if (windowProcess != GetCurrentProcessId()) {
    return TRUE;
  }

  if (GetWindow(handle, GW_OWNER) != (HWND)0) {
    return TRUE;
  }

  if (!IsWindowVisible(handle)) {
    return TRUE;
  }

  if (IsIconic(handle)) {
    return TRUE;
  }

  // In general, console windows can be main windows, but:
  // - never relevant for a graphics tablet
  // - debug builds of OpenKneeboard often create console windows for debug
  // messages
  if (handle == GetConsoleWindow()) {
    return TRUE;
  }

  RECT clientRect {};
  if (!GetClientRect(handle, &clientRect)) {
    return TRUE;
  }

  // Origin of client rect is always (0,0), bottom right is 1px outside the
  // window
  if (clientRect.right <= 1 || clientRect.bottom <= 1) {
    return TRUE;
  }

  *reinterpret_cast<HWND*>(param) = handle;
  return FALSE;
}

HWND FindMainWindow() {
  HWND ret {0};
  EnumWindows(&FindMainWindowCallback, reinterpret_cast<LPARAM>(&ret));
  return ret;
}

}// namespace OpenKneeboard
