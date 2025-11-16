// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include "FindMainWindow.hpp"

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
