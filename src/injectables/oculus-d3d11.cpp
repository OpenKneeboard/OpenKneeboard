#include <OpenKneeboard/dprint.h>
#include <windows.h>

#include "InjectedDLLMain.h"
#include "OculusD3D11Kneeboard.h"
#include "detours-ext.h"

using namespace OpenKneeboard;

namespace {
std::unique_ptr<OculusD3D11Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  DetourTransactionPushBegin();
  gInstance = std::make_unique<OculusD3D11Kneeboard>();
  DetourTransactionPopCommit();
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-Oculus-D3D11",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
