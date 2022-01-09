
#include <windows.h>

#include "IDXGISwapChainPresentHook.h"
#include "InjectedKneeboard.h"
#include "OculusD3D11Kneeboard.h"
#include "OculusFrameHook.h"
#include "OpenKneeboard/dprint.h"
#include "detours-ext.h"

using namespace OpenKneeboard;

namespace OpenKneeboard {

class AutoDetectKneeboard final : private OculusFrameHook,
                                  private IDXGISwapChainPresentHook {
 private:
  const uint64_t FLAG_OCULUS = 1 << 0;
  const uint64_t FLAG_D3D11 = 1 << 1;
  uint64_t mFlags = 0;
  uint64_t mFrames = 0;

  std::unique_ptr<InjectedKneeboard> mNext;

 public:
  void Unhook() {
    OculusFrameHook::Unhook();
    IDXGISwapChainPresentHook::Unhook();
    if (mNext) {
      mNext->Unhook();
    }
  }

 protected:
  virtual ovrResult OnEndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    decltype(&ovr_EndFrame) next) override {
    dprint("Detected Oculus");
    mFlags |= FLAG_OCULUS;
    auto ret
      = next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
    OculusFrameHook::Unhook();
    return ret;
  }

  virtual HRESULT OnPresent(
    UINT syncInterval,
    UINT flags,
    IDXGISwapChain* swapChain,
    decltype(&IDXGISwapChain::Present) next) override {
    if (mFrames == 0) {
      dprint("Detected D3D11");
    }
    mFrames++;
    mFlags |= FLAG_D3D11;
    auto ret = std::invoke(next, swapChain, syncInterval, flags);

    if (mFrames >= 2) {
      IDXGISwapChainPresentHook::UnhookAndCleanup();
      Next();
    }
    return ret;
  }

 private:
  void Next() {
    Unhook();

    if (mFlags == (FLAG_D3D11 | FLAG_OCULUS)) {
      mNext = std::make_unique<OculusD3D11Kneeboard>();
      return;
    }

    dprintf("Don't know how to create a kneeboard from autodetection flags {:#b}", mFlags);
  }
};

std::unique_ptr<AutoDetectKneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  DetourRestoreAfterWith();

  DetourTransactionPushBegin();
  gInstance = std::make_unique<AutoDetectKneeboard>();
  DetourTransactionPopCommit();
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace OpenKneeboard

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) {
    OpenKneeboard::DPrintSettings::Set({
      .Prefix = "OpenKneeboard-Autodetect",
    });
    dprintf("Attached to process.");
    // Create a new thread to avoid limitations on what we can do from DllMain
    CreateThread(nullptr, 0, &ThreadEntry, nullptr, 0, nullptr);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    dprint("Detaching from process...");
    DetourTransactionPushBegin();
    gInstance->Unhook();
    DetourTransactionPopCommit();
    dprint("Detached hooks, waiting for in-progress calls");
    // If, for example, hooked_ovr_EndFrame was being called when we
    // unhooked it, we need to let the current call finish.
    Sleep(500);
    dprint("Freeing resources");
    gInstance.reset();
    dprint("Cleanup complete.");
  }
  return TRUE;
}
