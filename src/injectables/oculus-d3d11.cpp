
// Copyright(c) 2017 Advanced Micro Devices, Inc. All rights reserved.
// Copyright(c) 2021-present Fred Emmott. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// https://github.com/GPUOpen-Tools/ocat used as reference; copyright notice
// above reflects copyright notices in source material.

#include <windows.h>

#include "OculusD3D11Kneeboard.h"
#include "OpenKneeboard/dprint.h"
#include "detours-ext.h"

using namespace OpenKneeboard;

static std::unique_ptr<OculusD3D11Kneeboard> gRenderer;

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) {
    OpenKneeboard::DPrintSettings::Set({
      .Prefix = "OpenKneeboard-Oculus-D3D11",
    });
    dprintf("Attached to process.");
    DetourRestoreAfterWith();

    DetourTransactionPushBegin();
    gRenderer = std::make_unique<OculusD3D11Kneeboard>();
    DetourTransactionPopCommit();
    dprint("Installed hooks.");
  } else if (dwReason == DLL_PROCESS_DETACH) {
    dprint("Detaching from process...");
    DetourTransactionPushBegin();
    gRenderer->Unhook();
    DetourTransactionPopCommit();
    dprint("Detached hooks, waiting for in-progress calls");
    // If, for example, hooked_ovr_EndFrame was being called when we
    // unhooked it, we need to let the current call finish.
    Sleep(500);
    dprint("Freeing resources");
    gRenderer.reset();
    dprint("Cleanup complete.");
  }
  return TRUE;
}
