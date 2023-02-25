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
#include "InjectedDLLMain.h"
#include "OculusD3D12Kneeboard.h"

using namespace OpenKneeboard;

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.Oculus.D3D12")
 * b24892c5-ac28-5d62-844b-0c4798cd6357
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.Oculus.D3D12",
  (0xb24892c5, 0xac28, 0x5d62, 0x84, 0x4b, 0x0c, 0x47, 0x98, 0xcd, 0x63, 0x57));
}// namespace OpenKneeboard

namespace {
std::unique_ptr<OculusD3D12Kneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<OculusD3D12Kneeboard>();
  dprintf(
    "----- OculusD3D12Kneeboard active at {:#018x} -----",
    (intptr_t)gInstance.get());
  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-Oculus-D3D12",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
