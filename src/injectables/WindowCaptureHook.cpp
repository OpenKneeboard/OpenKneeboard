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
#include <windowsx.h>

#include <atomic>
#include <optional>

#include "detours-ext.h"

using namespace OpenKneeboard;

namespace {
struct InjectedPoint {
  HWND mHwnd;
  POINT mClientPoint;
  POINT mScreenPoint;
};
}// namespace

static unsigned int gInjecting = 0;
static std::chrono::steady_clock::time_point gLastInjectedAt {};
static std::optional<InjectedPoint> gInjectedPoint;
static std::atomic_flag gHaveDetours;
static HWND gTopLevelWindow {};

static auto gPFN_GetForegroundWindow = &GetForegroundWindow;
static auto gPFN_SetForegroundWindow = &SetForegroundWindow;
static auto gPFN_GetCursorPos = &GetCursorPos;
static auto gPFN_ClientToScreen = &ClientToScreen;
static auto gPFN_ScreenToClient = &ScreenToClient;
static auto gPFN_GetFocus = &GetFocus;
static auto gPFN_IsWindowVisible = &IsWindowVisible;

static HWND gLastSetForegroundWindow {};

static bool ShouldInject() {
  return gInjecting
    || (std::chrono::steady_clock::now() - gLastInjectedAt
        > std::chrono::milliseconds(100));
}

static BOOL WINAPI SetForegroundWindow_Hook(HWND hwnd) {
  gLastSetForegroundWindow = hwnd;
  if (ShouldInject()) {
    return TRUE;
  }
  return gPFN_SetForegroundWindow(hwnd);
}

static HWND WINAPI GetForegroundWindow_Hook() {
  if (ShouldInject()) {
    return gLastSetForegroundWindow;
  }
  return gPFN_GetForegroundWindow();
}

static BOOL WINAPI GetCursorPos_Hook(LPPOINT lpPoint) {
  if (gInjectedPoint) {
    *lpPoint = gInjectedPoint->mScreenPoint;
    return TRUE;
  }
  return gPFN_GetCursorPos(lpPoint);
}

static BOOL WINAPI ClientToScreen_Hook(HWND hwnd, LPPOINT lpPoint) {
  if (!gInjectedPoint) {
    return gPFN_ClientToScreen(hwnd, lpPoint);
  }

  const auto screenPoint = gInjectedPoint->mScreenPoint;
  auto clientPoint = gInjectedPoint->mClientPoint;

  if (hwnd != gInjectedPoint->mHwnd) {
    MapWindowPoints(gInjectedPoint->mHwnd, hwnd, &clientPoint, 1);
  }

  *lpPoint = {
    screenPoint.x + lpPoint->x - clientPoint.x,
    screenPoint.y + lpPoint->y - clientPoint.y,
  };
  return TRUE;
}

static BOOL WINAPI ScreenToClient_Hook(HWND hwnd, LPPOINT lpPoint) {
  if (!gInjectedPoint) {
    return gPFN_ScreenToClient(hwnd, lpPoint);
  }

  const auto screenPoint = gInjectedPoint->mScreenPoint;
  auto clientPoint = gInjectedPoint->mClientPoint;

  if (hwnd != gInjectedPoint->mHwnd) {
    MapWindowPoints(gInjectedPoint->mHwnd, hwnd, &clientPoint, 1);
  }

  *lpPoint = {
    (lpPoint->x - clientPoint.x) - screenPoint.x,
    (lpPoint->y - clientPoint.y) - screenPoint.y,
  };
  return TRUE;
}

static HWND WINAPI GetFocus_Hook() {
  if (ShouldInject()) {
    return gLastSetForegroundWindow;
  }
  return gPFN_GetFocus();
}

static BOOL WINAPI IsWindowVisible_Hook(HWND hwnd) {
  if (!gTopLevelWindow) {
    return gPFN_IsWindowVisible(hwnd);
  }
  return GetAncestor(hwnd, GA_ROOTOWNER) == gTopLevelWindow;
}

static void InstallDetours() {
  if (gHaveDetours.test_and_set()) {
    return;
  }

  dprint("Installing detours");
  DetourTransaction transcation;
  DetourAttach(&gPFN_GetForegroundWindow, &GetForegroundWindow_Hook);
  DetourAttach(&gPFN_SetForegroundWindow, &SetForegroundWindow_Hook);
  DetourAttach(&gPFN_GetCursorPos, &GetCursorPos_Hook);
  DetourAttach(&gPFN_ClientToScreen, &ClientToScreen_Hook);
  DetourAttach(&gPFN_ScreenToClient, &ScreenToClient_Hook);
  DetourAttach(&gPFN_GetFocus, &GetFocus_Hook);
  DetourAttach(&gPFN_IsWindowVisible, &IsWindowVisible_Hook);
}

static void UninstallDetours() {
  if (!gHaveDetours.test()) {
    return;
  }
  gHaveDetours.clear();

  dprint("Removing detours");
  DetourTransaction transaction;
  DetourDetach(&gPFN_GetForegroundWindow, &GetForegroundWindow_Hook);
  DetourDetach(&gPFN_SetForegroundWindow, &SetForegroundWindow_Hook);
  DetourDetach(&gPFN_GetCursorPos, &GetCursorPos_Hook);
  DetourDetach(&gPFN_ClientToScreen, &ClientToScreen_Hook);
  DetourDetach(&gPFN_ScreenToClient, &ScreenToClient_Hook);
  DetourDetach(&gPFN_GetFocus, &GetFocus_Hook);
  DetourDetach(&gPFN_IsWindowVisible, &IsWindowVisible_Hook);
}

static bool ProcessControlMessage(
  HWND hwnd,
  unsigned int message,
  WPARAM wParam,
  LPARAM lParam) {
  static UINT sControlMessage
    = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);

  if (message != sControlMessage) {
    return false;
  }

  using WParam = WindowCaptureControl::WParam;
  switch (static_cast<WParam>(wParam)) {
    case WParam::Initialize:
      gTopLevelWindow = reinterpret_cast<HWND>(lParam);
      InstallDetours();
      break;
    case WParam::StartInjection:
      gInjecting++;
      gTopLevelWindow = reinterpret_cast<HWND>(lParam);
      gLastSetForegroundWindow = gTopLevelWindow;
      gLastInjectedAt = std::chrono::steady_clock::now();
      InstallDetours();
      break;
    case WParam::EndInjection:
      gInjecting--;
      if (!gInjecting) {
        gInjectedPoint = {};
        gLastSetForegroundWindow = {};
      }
      break;
  }

  return true;
}

static bool
ProcessMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (ProcessControlMessage(hwnd, message, wParam, lParam)) {
    return true;
  }

  if (!gInjecting) {
    return false;
  }

  switch (message) {
    case WM_MOUSEMOVE: {
      const POINT clientPoint {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      POINT screenPoint {clientPoint};
      gPFN_ClientToScreen(hwnd, &screenPoint);
      gInjectedPoint = InjectedPoint {
        .mHwnd = hwnd,
        .mClientPoint = clientPoint,
        .mScreenPoint = screenPoint,
      };
    }
      return false;
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      return true;
    default:
      return false;
  }
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
  GetMsgProc_WindowCaptureHook(int code, WPARAM wParam, LPARAM lParam) {
  auto& msg = *reinterpret_cast<PMSG>(lParam);

  if (ProcessMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam)) {
    msg.message = WM_NULL;
    return 0;
  }

  return CallNextHookEx(NULL, code, wParam, lParam);
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
  CallWndProc_WindowCaptureHook(int code, WPARAM wParam, LPARAM lParam) {
  auto& msg = *reinterpret_cast<PCWPSTRUCT>(lParam);
  if (ProcessMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam)) {
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
