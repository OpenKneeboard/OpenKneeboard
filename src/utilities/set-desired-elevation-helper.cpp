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

#include <OpenKneeboard/Elevation.hpp>

#include <Windows.h>

#include <OpenKneeboard/dprint.hpp>

#include <shellapi.h>

using namespace OpenKneeboard;

namespace OpenKneeboard {
/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.Elevation.Helper")
 * 4cd19abb-3b31-5e4e-ca98-75e403061214
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.Elevation.Helper",
  (0x4cd19abb, 0x3b31, 0x5e4e, 0xca, 0x98, 0x75, 0xe4, 0x03, 0x06, 0x12, 0x14));
}// namespace OpenKneeboard

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
