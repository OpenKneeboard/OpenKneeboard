// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "InjectedDLLMain.hpp"
#include "OculusD3D11Kneeboard.hpp"

using namespace OpenKneeboard;

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.Oculus.D3D11")
 * bce0dd2f-2946-509d-0079-54a2eb7e4cf9
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.Oculus.D3D11",
  (0xbce0dd2f, 0x2946, 0x509d, 0x00, 0x79, 0x54, 0xa2, 0xeb, 0x7e, 0x4c, 0xf9));
}// namespace OpenKneeboard

namespace {
std::unique_ptr<OculusD3D11Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<OculusD3D11Kneeboard>();
  dprint(
    "----- OculusD3D11Kneeboard active at {:#018x} -----",
    (intptr_t)gInstance.get());
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
