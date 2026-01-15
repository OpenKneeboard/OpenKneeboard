// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "detours-ext.hpp"

#include <OpenKneeboard/WindowCaptureControl.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>

#include <Windows.h>

#include <atomic>
#include <cstdlib>
#include <optional>

#include <windowsx.h>

using namespace OpenKneeboard;

namespace {
struct InjectedPoint {
  HWND mHwnd;
  POINT mScreenPoint;
};
}// namespace

static unsigned int gInjecting = 0;
static std::chrono::steady_clock::time_point gLastInjectedAt {};
static std::optional<InjectedPoint> gInjectedPoint;
static std::atomic_flag gHaveDetours;
static HWND gTopLevelWindow {};

auto& FunctionTable() {
  static struct {
    decltype(&GetForegroundWindow) gPFN_GetForegroundWindow
      = &GetForegroundWindow;
    decltype(&SetForegroundWindow) gPFN_SetForegroundWindow
      = &SetForegroundWindow;
    decltype(&GetCursorPos) gPFN_GetCursorPos = &GetCursorPos;
    decltype(&GetPhysicalCursorPos) gPFN_GetPhysicalCursorPos
      = &GetPhysicalCursorPos;
    decltype(&GetMessagePos) gPFN_GetMessagePos = &GetMessagePos;
    decltype(&GetFocus) gPFN_GetFocus = &GetFocus;
    decltype(&IsWindowEnabled) gPFN_IsWindowEnabled = &IsWindowEnabled;
    decltype(&IsWindowVisible) gPFN_IsWindowVisible = &IsWindowVisible;
    decltype(&WindowFromPoint) gPFN_WindowFromPoint = &WindowFromPoint;
  } sData {};
  return sData;
}

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
  return FunctionTable().gPFN_SetForegroundWindow(hwnd);
}

static HWND WINAPI GetForegroundWindow_Hook() {
  if (ShouldInject()) {
    return gLastSetForegroundWindow;
  }
  return FunctionTable().gPFN_GetForegroundWindow();
}

static BOOL WINAPI GetCursorPos_Hook(LPPOINT lpPoint) {
  if (gInjectedPoint) {
    *lpPoint = gInjectedPoint->mScreenPoint;
    return TRUE;
  }
  return FunctionTable().gPFN_GetCursorPos(lpPoint);
}

static BOOL WINAPI GetPhysicalCursorPos_Hook(LPPOINT lpPoint) {
  // TODO: test with multiple DPIs
  if (gInjectedPoint) {
    *lpPoint = gInjectedPoint->mScreenPoint;
    return TRUE;
  }
  return FunctionTable().gPFN_GetPhysicalCursorPos(lpPoint);
}

static DWORD WINAPI GetMessagePos_Hook() {
  if (gInjectedPoint) {
    return gInjectedPoint->mScreenPoint.x
      + (gInjectedPoint->mScreenPoint.y << sizeof(DWORD) * 4);
  }
  return FunctionTable().gPFN_GetMessagePos();
}

static HWND WINAPI GetFocus_Hook() {
  if (ShouldInject()) {
    return gLastSetForegroundWindow;
  }
  return FunctionTable().gPFN_GetFocus();
}

static BOOL WINAPI IsWindowEnabled_Hook(HWND hwnd) {
  if (ShouldInject()) {
    return GetAncestor(hwnd, GA_ROOTOWNER) == gTopLevelWindow;
  }
  return FunctionTable().gPFN_IsWindowEnabled(hwnd);
}

static BOOL WINAPI IsWindowVisible_Hook(HWND hwnd) {
  if (!gTopLevelWindow) {
    return FunctionTable().gPFN_IsWindowVisible(hwnd);
  }
  return GetAncestor(hwnd, GA_ROOTOWNER) == gTopLevelWindow;
}

static HWND WINAPI WindowFromPoint_Hook(POINT point) {
  if (gInjectedPoint) {
    return gInjectedPoint->mHwnd;
  }
  return FunctionTable().gPFN_WindowFromPoint(point);
}

static void InstallDetours() {
  if (gHaveDetours.test_and_set()) {
    return;
  }

  dprint("Installing detours");
  const DetourTransaction transaction;
  auto& funcs = FunctionTable();
  DetourAttach(&funcs.gPFN_GetForegroundWindow, &GetForegroundWindow_Hook);
  DetourAttach(&funcs.gPFN_SetForegroundWindow, &SetForegroundWindow_Hook);
  DetourAttach(&funcs.gPFN_GetCursorPos, &GetCursorPos_Hook);
  DetourAttach(&funcs.gPFN_GetPhysicalCursorPos, &GetPhysicalCursorPos_Hook);
  DetourAttach(&funcs.gPFN_GetMessagePos, &GetMessagePos_Hook);
  DetourAttach(&funcs.gPFN_GetFocus, &GetFocus_Hook);
  DetourAttach(&funcs.gPFN_IsWindowEnabled, &IsWindowEnabled_Hook);
  DetourAttach(&funcs.gPFN_IsWindowVisible, &IsWindowVisible_Hook);
  DetourAttach(&funcs.gPFN_WindowFromPoint, &WindowFromPoint_Hook);
}

static void UninstallDetours() {
  if (!gHaveDetours.test()) {
    return;
  }
  gHaveDetours.clear();

  dprint("Removing detours");
  const DetourTransaction transaction;
  auto& funcs = FunctionTable();
  DetourDetach(&funcs.gPFN_GetForegroundWindow, &GetForegroundWindow_Hook);
  DetourDetach(&funcs.gPFN_SetForegroundWindow, &SetForegroundWindow_Hook);
  DetourDetach(&funcs.gPFN_GetCursorPos, &GetCursorPos_Hook);
  DetourDetach(&funcs.gPFN_GetPhysicalCursorPos, &GetPhysicalCursorPos_Hook);
  DetourDetach(&funcs.gPFN_GetMessagePos, &GetMessagePos_Hook);
  DetourDetach(&funcs.gPFN_GetFocus, &GetFocus_Hook);
  DetourDetach(&funcs.gPFN_IsWindowEnabled, &IsWindowEnabled_Hook);
  DetourDetach(&funcs.gPFN_IsWindowVisible, &IsWindowVisible_Hook);
  DetourDetach(&funcs.gPFN_WindowFromPoint, &WindowFromPoint_Hook);
}

static bool ProcessControlMessage(
  HWND,
  unsigned int message,
  const WPARAM wParam,
  const LPARAM lParam) {
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
      ClientToScreen(hwnd, &screenPoint);
      gInjectedPoint = InjectedPoint {
        .mHwnd = hwnd,
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

  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity,
    "GetMsgProc",
    TraceLoggingValue(msg.message, "message"),
    TraceLoggingValue(msg.wParam, "wParam"),
    TraceLoggingValue(msg.lParam, "lParam"));

  if (ProcessMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam)) {
    msg.message = WM_NULL;
    TraceLoggingWriteStop(
      activity, "GetMsgProc", TraceLoggingValue("Hooked", "Result"));
    return 0;
  }

  TraceLoggingWriteStop(
    activity, "GetMsgProc", TraceLoggingValue("CallNextHookEx", "Result"));
  return CallNextHookEx(NULL, code, wParam, lParam);
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
CallWndProc_WindowCaptureHook(int code, WPARAM wParam, LPARAM lParam) {
  auto& msg = *reinterpret_cast<PCWPSTRUCT>(lParam);

  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity,
    "CallWndProc",
    TraceLoggingValue(msg.message, "message"),
    TraceLoggingValue(msg.wParam, "wParam"),
    TraceLoggingValue(msg.lParam, "lParam"));

  if (ProcessMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam)) {
    TraceLoggingWriteStop(
      activity, "CallWndProc", TraceLoggingValue("Hooked", "Result"));
    return 0;
  }

  TraceLoggingWriteStop(
    activity, "CallWndProc", TraceLoggingValue("CallNextHook", "Result"));
  return CallNextHookEx(NULL, code, wParam, lParam);
}

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.WindowCaptureHook")
 * 2f381a1b-6486-55d8-ee5a-3cc04e8df79d
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.WindowCaptureHook",
  (0x2f381a1b, 0x6486, 0x55d8, 0xee, 0x5a, 0x3c, 0xc0, 0x4e, 0x8d, 0xf7, 0x9d));

}// namespace OpenKneeboard

static std::wstring GetProgramPath() {
  wchar_t buffer[1024];
  const auto pathLen = GetModuleFileNameW(NULL, buffer, std::size(buffer));
  return {buffer, pathLen};
}

BOOL WINAPI DllMain(HINSTANCE, const DWORD dwReason, const LPVOID reserved) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(gTraceProvider);
      DPrintSettings::Set({.prefix = "WindowCaptureHook"});

      dprint(
        L"Attached to {}-bit process {} ({})",
        sizeof(void*) * 8,
        GetProgramPath(),
        GetCurrentProcessId());
      break;
    case DLL_PROCESS_DETACH:
      dprint(
        L"Detaching from process {} ({})",
        GetProgramPath(),
        GetCurrentProcessId());
      TraceLoggingUnregister(gTraceProvider);
      // Per https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain :
      //
      // - lpreserved is NULL if DLL is being unloaded, non-null if the process
      //   is terminating.
      // - if the process is terminating, it is unsafe to attempt to cleanup
      //   heap resources, and they *should* leave resource reclamation to the
      //   kernel; our destructors etc may depend on dlls that have already
      //   been unloaded.
      if (!reserved) {
        UninstallDetours();
      }
      break;
  }
  return TRUE;
}
