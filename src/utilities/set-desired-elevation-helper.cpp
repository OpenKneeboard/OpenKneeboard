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

/** A separate process to register the OpenXR layer, outside of the
 * MSIX sandbox.
 *
 * If done from the main process, the registry write will be app-specific.
 */

#include <OpenKneeboard/Elevation.h>
#include <OpenKneeboard/dprint.h>
#include <Windows.h>
#include <shellapi.h>

using namespace OpenKneeboard;

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
  DPrintSettings::Set({
    .prefix = "set-desired-elevation-helper",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });

  int argc = 0;
  auto argv = CommandLineToArgvW(commandLine, &argc);
  if (argc != 1) {
    dprintf("Invalid arguments ({}):", argc);
    for (int i = 0; i < argc; ++i) {
      dprintf(L"argv[{}]: {}", i, argv[i]);
    }
    return 1;
  }
  const auto mode = wcstoul(argv[0], nullptr, 10);
  dprintf("Setting desired elevation to {}", mode);
  SetDesiredElevation(static_cast<DesiredElevation>(mode));
  return 0;
}
