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
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/WindowCaptureControl.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <Windows.h>

namespace OpenKneeboard::WindowCaptureControl {

Handles InstallHooks(HWND hwnd) {
  DWORD processID {};
  Handles ret {};

  auto threadID = GetWindowThreadProcessId(hwnd, &processID);
  if (processID == GetCurrentProcessId()) {
    dprint("Cowardly refusing to move my own mouse cursor.");
    return {};
  }
  if (!threadID) {
    dprint("Failed to find thread for window.");
    return {};
  }

  const auto hookPath = (RuntimeFiles::GetInstallationDirectory()
                         / RuntimeFiles::WINDOW_CAPTURE_HOOK_DLL)
                          .wstring();

  dprint(L"Loading hook library: {}", hookPath);

  ret.mLibrary.reset(LoadLibraryW(hookPath.c_str()));
  if (!ret.mLibrary) {
    dprint("Failed to load hook library: {}", GetLastError());
  }

  auto msgProc
    = GetProcAddress(ret.mLibrary.get(), "GetMsgProc_WindowCaptureHook");
  if (!msgProc) {
    dprint("Failed to find msgProc hook address: {}", GetLastError());
  }
  auto wndProc
    = GetProcAddress(ret.mLibrary.get(), "CallWndProc_WindowCaptureHook");
  if (!msgProc) {
    dprint("Failed to find wndProc hook address: {}", GetLastError());
  }

  ret.mMessageHook.reset(SetWindowsHookExW(
    WH_GETMESSAGE,
    reinterpret_cast<HOOKPROC>(msgProc),
    ret.mLibrary.get(),
    threadID));
  if (!ret.mMessageHook) {
    dprint("Failed to set WH_GETMESSAGE: {}", GetLastError());
  }
  ret.mWindowProcHook.reset(SetWindowsHookExW(
    WH_CALLWNDPROC,
    reinterpret_cast<HOOKPROC>(wndProc),
    ret.mLibrary.get(),
    threadID));
  if (!ret.mWindowProcHook) {
    dprint("Failed to set WH_CALLWNDPROC: {}", GetLastError());
  }
  return ret;
}

bool Handles::IsValid() const {
  return mLibrary && mMessageHook && mWindowProcHook;
}

}// namespace OpenKneeboard::WindowCaptureControl
