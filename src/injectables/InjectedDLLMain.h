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
    dprintf("Attached to process.");

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
