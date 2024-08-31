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
#include <OpenKneeboard/APIEvent.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/tracing.hpp>

using namespace OpenKneeboard;

#include <OpenKneeboard/scope_exit.hpp>

#include <Windows.h>
#include <shellapi.h>

#include <cstdlib>

namespace OpenKneeboard {
/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.PluginTabAction")
 * 0d024362-97d7-5ba3-7e08-7870f0ea3357
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.PluginTabAction",
  (0x0d024362, 0x97d7, 0x5ba3, 0x7e, 0x08, 0x78, 0x70, 0xf0, 0xea, 0x33, 0x57));

}// namespace OpenKneeboard

// We only need a standard `main()` function, but using wWinMain prevents
// a window/task bar entry from temporarily appearing
int WINAPI wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  PWSTR pCmdLine,
  int nCmdShow) {
  TraceLoggingRegister(OpenKneeboard::gTraceProvider);
  TraceLoggingWrite(
    OpenKneeboard::gTraceProvider,
    "Invocation",
    TraceLoggingThisExecutable(),
    TraceLoggingValue(GetCommandLineW(), "Command Line"));
  OpenKneeboard::scope_exit etwGuard(
    []() { TraceLoggingUnregister(OpenKneeboard::gTraceProvider); });

  DPrintSettings::Set({
    .prefix = "Plugin-Tab-Action",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(pCmdLine, &argc);

  if (argc < 1 || argc > 2) {
    dprint("Usage: ACTION_ID [CUSTOM_DATA]");
    return 0;
  }

  PluginTabCustomActionEvent ev {.mActionID = winrt::to_string(argv[0])};

  if (argc >= 2) {
    try {
      ev.mExtraData = nlohmann::json::parse(winrt::to_string(argv[1]));
    } catch (const nlohmann::json::exception& e) {
      dprint("Failed to parse JSON customData: {}", e.what());
      return 1;
    }
  }

  const nlohmann::json payload = ev;

  APIEvent::Send({APIEvent::EVT_PLUGIN_TAB_CUSTOM_ACTION, payload.dump()});

  return 0;
}
