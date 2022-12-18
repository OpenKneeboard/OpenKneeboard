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

#include <OpenKneeboard/ConsoleLoopCondition.h>
#include <OpenKneeboard/WindowCaptureControl.h>
#include <OpenKneeboard/dprint.h>
#include <shellapi.h>
#include <shims/Windows.h>

#include <cstdlib>

using namespace OpenKneeboard;

// We only need a standard `main()` function, but using wWinMain prevents
// a window/task bar entry from temporarily appearing
int WINAPI wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  PWSTR pCmdLine,
  int nCmdShow) {
  DPrintSettings::Set({
    .prefix = "WindowCaptureHook-helper",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(pCmdLine, &argc);

  if (argc != 1) {
    dprint("Usage: HWND");
    return 1;
  }

  const auto hwnd = reinterpret_cast<HWND>(_wcstoui64(argv[0], nullptr, 10));
  if (!hwnd) {
    dprint("Unable to parse an hwnd");
    return 1;
  }

  auto handles = WindowCaptureControl::InstallHooks(hwnd);
  if (!handles.IsValid()) {
    dprint("Failed to attach to hwnd");
    return 1;
  }

  ConsoleLoopCondition loop;
  while (loop.Sleep(std::chrono::seconds(1))) {
    // do nothing
  }
  return 0;
}
