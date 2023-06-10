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

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

#include <Windows.h>

#include <Msi.h>

using namespace OpenKneeboard;

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.Repair.Helper")
 * 6d8fd0b9-e465-5397-f126-ad45a697d226
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.Repair.Helper",
  (0x6d8fd0b9, 0xe465, 0x5397, 0xf1, 0x26, 0xad, 0x45, 0xa6, 0x97, 0xd2, 0x26));
}// namespace OpenKneeboard

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
  TraceLoggingRegister(gTraceProvider);
  const scope_guard unregisterTraceProvider(
    []() { TraceLoggingUnregister(gTraceProvider); });
  DPrintSettings::Set({
    .prefix = "Installation-Helper",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });

  wchar_t productId[39];
  auto ret = MsiEnumRelatedProductsW(
    L"{843c9331-0610-4ab1-9cf9-5305c896fb5b}", 0, 0, productId);
  if (ret != ERROR_SUCCESS) {
    dprintf("Failed to find MSI product ID: {}", ret);
    return 0;
  }

  dprintf(L"Repairing MSI product ID {}", productId);
  ret = MsiReinstallProductW(productId, REINSTALLMODE_FILEEXACT);
  if (ret != ERROR_SUCCESS) {
    dprintf("MSI product repair failed: {}", ret);
    return 0;
  }
  dprint("MSI product repaired.");

  DWORD repairOnNextRun {0};
  RegSetKeyValueW(
    HKEY_LOCAL_MACHINE,
    RegistrySubKey,
    L"RepairOnNextRun",
    REG_DWORD,
    &repairOnNextRun,
    sizeof(repairOnNextRun));

  return 0;
}
