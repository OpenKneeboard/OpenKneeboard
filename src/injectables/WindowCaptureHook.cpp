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
#include <OpenKneeboard/WindowCaptureControl.h>
#include <OpenKneeboard/config.h>
#include <shims/Windows.h>

using namespace OpenKneeboard;

static unsigned int gDisableMouseLeave = 0;

static bool
ProcessControlMessage(unsigned int message, WPARAM wParam, LPARAM lParam) {
  static UINT sControlMessage
    = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);

  if (message != sControlMessage) {
    return false;
  }

  switch (wParam) {
    case static_cast<WPARAM>(
      WindowCaptureControl::WParam::Disable_WM_MOUSELEAVE):
      gDisableMouseLeave++;
      break;
    case static_cast<WPARAM>(
      WindowCaptureControl::WParam::Enable_WM_MOUSELEAVE):
      gDisableMouseLeave--;
      break;
  }

  return true;
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
  GetMsgProc_WindowCaptureHook(int code, WPARAM wParam, LPARAM lParam) {
  auto& msg = *reinterpret_cast<PMSG>(lParam);

  if (ProcessControlMessage(msg.message, msg.wParam, msg.lParam)) {
    msg.message = WM_NULL;
    return 0;
  }

  if (gDisableMouseLeave && msg.message == WM_MOUSELEAVE) {
    msg.message = WM_NULL;
    return 0;
  }

  return CallNextHookEx(NULL, code, wParam, lParam);
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
  CallWndProc_WindowCaptureHook(int code, WPARAM wParam, LPARAM lParam) {
  auto& msg = *reinterpret_cast<PCWPSTRUCT>(lParam);
  if (ProcessControlMessage(msg.message, msg.wParam, msg.lParam)) {
    return 0;
  }
  return CallNextHookEx(NULL, code, wParam, lParam);
}
