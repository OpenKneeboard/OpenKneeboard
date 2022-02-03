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
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/WintabTablet.h>
#include <OpenKneeboard/dprint.h>
#include <Windows.h>

#include <functional>
#include <stop_token>
#include <thread>

#include "InjectedDLLMain.h"

namespace OpenKneeboard {

/** Work around for the lack of background access in the wintab API.
 *
 * When injected, this DLL will forward wintab events back to the OpenKneeboard
 * window.
 */
class TabletProxy final {
 public:
  TabletProxy();
  ~TabletProxy();

  TabletProxy(const TabletProxy&) = delete;
  TabletProxy& operator=(const TabletProxy&) = delete;

 private:
  void Initialize();
  void Detach();
  void WatchForEnvironmentChanges(std::stop_token);

  static HWND FindMainWindow();
  bool mInitialized = false;
  std::jthread mWatchThread;

  static WNDPROC mWindowProc;
  static HWND mTargetWindow;
  static SHM::Reader mSHM;
  static std::unique_ptr<WintabTablet> mTablet;

  static LRESULT CALLBACK
  HookedWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

SHM::Reader TabletProxy::mSHM;
WNDPROC TabletProxy::mWindowProc;
HWND TabletProxy::mTargetWindow;
std::unique_ptr<WintabTablet> TabletProxy::mTablet;

TabletProxy::TabletProxy() {
  Initialize();

  mWatchThread
    = {std::bind_front(&TabletProxy::WatchForEnvironmentChanges, this)};
}

TabletProxy::~TabletProxy() {
  mWatchThread.request_stop();
  if (mWatchThread.joinable()) {
    mWatchThread.join();
  }

  Detach();
}

void TabletProxy::Initialize() {
  if (mInitialized) {
    return;
  }

  mTargetWindow = FindMainWindow();
  if (!mTargetWindow) {
    return;
  }
  char title[256];
  auto titleLen = GetWindowTextA(mTargetWindow, title, sizeof(title));
  dprintf("Main window: {}", std::string_view(title, titleLen));

  mTablet = std::make_unique<WintabTablet>(mTargetWindow);
  if (!*mTablet) {
    return;
  }

  mWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
    mTargetWindow,
    GWLP_WNDPROC,
    reinterpret_cast<LONG_PTR>(&HookedWindowProc)));
  if (!mWindowProc) {
    dprintf("Failed to install windowproc: {}", GetLastError());
    OPENKNEEBOARD_BREAK;
    return;
  }
  mInitialized = true;
}

void TabletProxy::WatchForEnvironmentChanges(std::stop_token stop) {
  while (!(mInitialized || stop.stop_requested())) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    Initialize();
  }
  // Maybe we previously attached to a splash screen
  while (!stop.stop_requested()) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (FindMainWindow() == mTargetWindow) {
      continue;
    }
    Detach();
    Initialize();
  }
}

void TabletProxy::Detach() {
  if (!mInitialized) {
    return;
  }

  SetWindowLongPtr(
    mTargetWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(mWindowProc));
  mTargetWindow = {};
  mWindowProc = {};
  mTablet = {};
  mInitialized = false;
}

LRESULT TabletProxy::HookedWindowProc(
  HWND hwnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  if (hwnd == mTargetWindow && mTablet->CanProcessMessage(uMsg)) {
    auto snapshot = mSHM.MaybeGet();
    if (snapshot && snapshot.GetConfig().feederWindow) {
      SendNotifyMessage(
        snapshot.GetConfig().feederWindow, uMsg, wParam, lParam);
    }
    return S_OK;
  }

  return CallWindowProc(mWindowProc, hwnd, uMsg, wParam, lParam);
}

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

  // In general, console windows can be main windows, but:
  // - never relevant for a graphics tablet
  // - debug builds of OpenKneeboard often create console windows for debug
  // messages
  if (handle == GetConsoleWindow()) {
    return TRUE;
  }

  *reinterpret_cast<HWND*>(param) = handle;
  return FALSE;
}

HWND TabletProxy::FindMainWindow() {
  HWND ret {0};
  EnumWindows(&FindMainWindowCallback, reinterpret_cast<LPARAM>(&ret));
  return ret;
}

}// namespace OpenKneeboard

using namespace OpenKneeboard;

namespace {

std::unique_ptr<TabletProxy> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<TabletProxy>();
  dprint("Installed Tablet Proxy");
  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-TabletProxy",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
