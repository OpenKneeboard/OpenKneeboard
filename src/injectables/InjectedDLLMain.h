#pragma once

#include <OpenKneeboard/dprint.h>
#include <Windows.h>
#include "detours-ext.h"

namespace OpenKneeboard {

template<class T>
BOOL InjectedDLLMain(
  const char* logPrefix,
  std::unique_ptr<T>& instance,
  LPTHREAD_START_ROUTINE entrypoint,
  HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) {
    DetourRestoreAfterWith();

    DPrintSettings::Set({
      .prefix = logPrefix
    });
    dprintf("Attached to process.");
    // Create a new thread to avoid limitations on what we can do from DllMain
    //
    // For example, we can't call `CoCreateInstance()` or DirectX factory functions
    // from DllMain
    CreateThread(nullptr, 0, entrypoint, nullptr, 0, nullptr);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    dprint("Detaching from process...");
    DetourTransactionPushBegin();
    instance->Unhook();
    DetourTransactionPopCommit();
    dprint("Detached hooks, waiting for in-progress calls");
    // If a hook function was in progress when we unhooked it, we need to let the
    // current call finish
    Sleep(500);
    dprint("Freeing resources");
    instance.reset();
    dprint("Cleanup complete.");
  }
  return TRUE;
}

}
