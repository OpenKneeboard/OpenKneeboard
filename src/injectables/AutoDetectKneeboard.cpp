
#include <Unknwn.h>
#include <d3d12.h>
#include <windows.h>
#include <winrt/base.h>

#include "IDXGISwapChainPresentHook.h"
#include "InjectedDLLMain.h"
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
  const uint64_t FLAG_D3D11 = 1 << 0;
  const uint64_t FLAG_D3D12 = 1 << 1;
  const uint64_t FLAG_OCULUS = 1ui64 << 32;
  const uint64_t FLAG_STEAMVR = 1ui64 << 33;

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

  void SetD3DFlags(IDXGISwapChain* swapChain) {
    dprint("Found DXGI...");

    winrt::com_ptr<ID3D11Device> d3d11;
    swapChain->GetDevice(IID_PPV_ARGS(d3d11.put()));
    if (d3d11) {
      dprint("... found D3D11");
      mFlags |= FLAG_D3D11;
      return;
    }

    winrt::com_ptr<ID3D12Device> d3d12;
    swapChain->GetDevice(IID_PPV_ARGS(d3d12.put()));
    if (d3d12) {
      dprint("... found D3D12");
      mFlags |= FLAG_D3D12;
      return;
    }

    dprint("... but couldn't figure out the DirectX version");
  }

  virtual HRESULT OnPresent(
    UINT syncInterval,
    UINT flags,
    IDXGISwapChain* swapChain,
    decltype(&IDXGISwapChain::Present) next) override {
    if (mFrames == 0) {
      SetD3DFlags(swapChain);
    }
    mFrames++;
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

    CheckForSteamVR();

    if ((mFlags & FLAG_STEAMVR)) {
      dprint("Doing nothing as SteamVR is in-process");
      return;
    }

    if (mFlags == (FLAG_D3D11 | FLAG_OCULUS)) {
      mNext = std::make_unique<OculusD3D11Kneeboard>();
      return;
    }

    dprintf(
      "Don't know how to create a kneeboard from autodetection flags {:#b}",
      mFlags);
  }

  void CheckForSteamVR() {
    // Not using OpenVR as we would need to load openvr_api.dll in process
    auto f = DetourFindFunction("vrclient_x64.dll", "VRClientCoreFactory");
    if (f) {
      mFlags |= FLAG_STEAMVR;
    }
  }
};

}// namespace OpenKneeboard

namespace {

std::unique_ptr<AutoDetectKneeboard> gInstance;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  DetourTransactionPushBegin();
  gInstance = std::make_unique<AutoDetectKneeboard>();
  DetourTransactionPopCommit();
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  return InjectedDLLMain(
    "OpenKneeboard-Autodetect",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
