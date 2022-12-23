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
#include <OpenKneeboard/dprint.h>
#include <shims/Windows.h>

#include <atomic>

#include "detours-ext.h"

using namespace OpenKneeboard;

static unsigned int gDisableMouseLeave = 0;
static std::atomic_flag gHaveDetours;
static auto gPFN_SetForegroundWindow = &SetForegroundWindow;

static BOOL WINAPI SetForegroundWindow_Hook(HWND hwnd) {
  if (gDisableMouseLeave) {
    dprint("Suppressing SetForegroundWindow()");
    return true;
  }
  return gPFN_SetForegroundWindow(hwnd);
}

static void InstallDetours() {
  if (gHaveDetours.test_and_set()) {
    return;
  }

  dprint("Installing detours");
  DetourSingleAttach(&gPFN_SetForegroundWindow, &SetForegroundWindow_Hook);
}

static void UninstallDetours() {
  if (!gHaveDetours.test()) {
    return;
  }

  dprint("Removing detours");
  DetourSingleDetach(&gPFN_SetForegroundWindow, &SetForegroundWindow_Hook);
}

static bool
ProcessControlMessage(unsigned int message, WPARAM wParam, LPARAM lParam) {
  InstallDetours();
  static UINT sControlMessage
    = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);

  if (message != sControlMessage) {
    return false;
  }

  using WParam = WindowCaptureControl::WParam;
  switch (static_cast<WParam>(wParam)) {
    case WParam::Disable_WM_MOUSELEAVE:
      gDisableMouseLeave++;
      break;
    case WParam::Enable_WM_MOUSELEAVE:
      gDisableMouseLeave--;
      break;
  }

  return true;
}

static bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
  if (ProcessControlMessage(message, wParam, lParam)) {
    return true;
  }

  if (!gDisableMouseLeave) {
    return false;
  }

  switch (message) {
    case WM_ACTIVATE:
      return true;
    case WM_MOUSELEAVE:
      return true;
    default:
      return false;
  }
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
  GetMsgProc_WindowCaptureHook(int code, WPARAM wParam, LPARAM lParam) {
  auto& msg = *reinterpret_cast<PMSG>(lParam);

  if (ProcessMessage(msg.message, msg.wParam, msg.lParam)) {
    msg.message = WM_NULL;
    return 0;
  }

  return CallNextHookEx(NULL, code, wParam, lParam);
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
  CallWndProc_WindowCaptureHook(int code, WPARAM wParam, LPARAM lParam) {
  auto& msg = *reinterpret_cast<PCWPSTRUCT>(lParam);
  if (ProcessMessage(msg.message, msg.wParam, msg.lParam)) {
    return 0;
  }
  return CallNextHookEx(NULL, code, wParam, lParam);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  // Per https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain :
  //
  // - lpreserved is NULL if DLL is being unloaded, non-null if the process
  //   is terminating.
  // - if the process is terminating, it is unsafe to attempt to cleanup
  //   heap resources, and they *should* leave resource reclamation to the
  //   kernel; our destructors etc may depend on dlls that have already
  //   been unloaded.
  if (dwReason == DLL_PROCESS_DETACH && !reserved) {
    UninstallDetours();
  }
  return TRUE;
}
