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
#include <windowsx.h>

#include <atomic>
#include <format>
#include <optional>

#include "detours-ext.h"

using namespace OpenKneeboard;

static unsigned int gInjecting = 0;
static std::atomic_flag gInstalledDetours;
static decltype(&GetCursorPos) gPFN_GetCursorPos = nullptr;
static decltype(&SetCursorPos) gPFN_SetCursorPos = nullptr;
std::optional<POINT> gInjectedPoint;
static HINSTANCE gInstance = nullptr;

static BOOL WINAPI GetCursorPos_Hook(LPPOINT point) {
  if (gInjectedPoint) {
    *point = *gInjectedPoint;
    return true;
  }
  return gPFN_GetCursorPos(point);
}

static BOOL WINAPI SetCursorPos_Hook(int x, int y) {
  return true;
}

static void InstallDetours() {
  if (gInstalledDetours.test_and_set()) {
    return;
  }

  wchar_t pathBuf[MAX_PATH];
  GetModuleFileNameW(static_cast<HMODULE>(gInstance), pathBuf, MAX_PATH);
  // Make sure we're never unloaded; once the detour's installed, it'll crash.
  LoadLibraryW(pathBuf);

  gPFN_GetCursorPos = &GetCursorPos;
  gPFN_SetCursorPos = &SetCursorPos;
  DetourTransaction transaction;
  DetourAttach(&gPFN_GetCursorPos, &GetCursorPos_Hook);
  DetourAttach(&gPFN_SetCursorPos, &SetCursorPos_Hook);
}

static bool
ProcessControlMessage(unsigned int message, WPARAM wParam, LPARAM lParam) {
  static const UINT sControlMessage {
    RegisterWindowMessageW(WindowCaptureControl::WindowMessageName)};

  InstallDetours();

  if (message != sControlMessage) {
    return false;
  }

  using WParam = WindowCaptureControl::WParam;
  switch (static_cast<WParam>(wParam)) {
    case WParam::StartInjection:
      gInjecting++;
      break;
    case WParam::EndInjection:
      gInjecting--;
      break;
  }

  return true;
}

static bool
ProcessMessage(HWND hwnd, unsigned int message, WPARAM wParam, LPARAM lParam) {
  if (ProcessControlMessage(message, wParam, lParam)) {
    return true;
  }

  if (gInjecting) {
    switch (message) {
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_MOUSEMOVE: {
        POINT point {
          GET_X_LPARAM(lParam),
          GET_Y_LPARAM(lParam),
        };
        if (ClientToScreen(hwnd, &point)) {
          gInjectedPoint = point;
        }
        break;
      }
      case WM_MOUSELEAVE:
        return true;
    }
  } else {
    switch (message) {
      case WM_MOUSEMOVE:
        gInjectedPoint = {};
        break;
    }
  }

  return false;
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
  if (hinst) {
    gInstance = hinst;
  }
  return TRUE;
}
