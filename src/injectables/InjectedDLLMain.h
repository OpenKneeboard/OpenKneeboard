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

#include <OpenKneeboard/dprint.h>
#include <Windows.h>

#include "detours-ext.h"

namespace OpenKneeboard {

template <class T>
BOOL InjectedDLLMain(
  const char* logPrefix,
  std::unique_ptr<T>& instance,
  LPTHREAD_START_ROUTINE entrypoint,
  HINSTANCE hinst,
  DWORD dwReason,
  LPVOID reserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) {
    DPrintSettings::Set({.prefix = logPrefix});
    std::wstring fullDllPath;
    fullDllPath.resize(MAX_PATH);
    {
      auto len
        = GetModuleFileNameW(hinst, fullDllPath.data(), fullDllPath.size());
      fullDllPath.resize(len);
    }
    dprintf(L"Attached to process: {}", fullDllPath);

    DetourRestoreAfterWith();
    DisableThreadLibraryCalls(hinst);

    // Create a new thread to avoid limitations on what we can do from DllMain
    //
    // For example, we can't call `CoCreateInstance()` or DirectX factory
    // functions from DllMain
    dprint("Spawning init thread...");
    CreateThread(nullptr, 0, entrypoint, nullptr, 0, nullptr);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    if (reserved) {
      // Per https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain :
      //
      // - lpreserved is NULL if DLL is being loaded, non-null if the process
      //   is terminating.
      // - if the process is terminating, it is unsafe to attempt to cleanup
      //   heap resources, and they *should* leave resource reclamation to the
      //   kernel; our destructors etc may depend on dlls that have already
      //   been unloaded.
      auto leaked = instance.release();
      dprint("Process is shutting down, leaving cleanup for the kernel.");
      return TRUE;
    }
    dprint("Detaching from process...");
    instance.reset();
    dprint("Cleanup complete.");
  }
  return TRUE;
}

}// namespace OpenKneeboard
