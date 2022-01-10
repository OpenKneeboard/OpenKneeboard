
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>
#include <Unknwn.h>
#include <d3d11.h>
#include <d3d12.h>
#include <windows.h>
#include <winrt/base.h>

#include "IDXGISwapChainPresentHook.h"
#include "IVRCompositorSubmitHook.h"
#include "InjectedDLLMain.h"
#include "OculusFrameHook.h"
#include "detours-ext.h"

using namespace OpenKneeboard;

namespace OpenKneeboard {

class AutoDetectKneeboard final : private OculusFrameHook,
                                  private IDXGISwapChainPresentHook,
                                  private IVRCompositorSubmitHook {
 private:
  const uint64_t FLAG_D3D11 = 1 << 0;
  const uint64_t FLAG_D3D12 = 1 << 1;
  const uint64_t FLAG_OCULUS = 1ui64 << 32;
  const uint64_t FLAG_STEAMVR = 1ui64 << 33;

  HMODULE mThisModule = nullptr;
  uint64_t mFlags = 0;
  uint64_t mFrames = 0;

 public:
  AutoDetectKneeboard() = delete;

  explicit AutoDetectKneeboard(HMODULE self) : mThisModule(self) {
  }

  void Unhook() {
    OculusFrameHook::Unhook();
    IDXGISwapChainPresentHook::Unhook();
    IVRCompositorSubmitHook::Unhook();
  }

 protected:
  virtual ovrResult OnOVREndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    const decltype(&ovr_EndFrame)& next) override {
    dprint("Detected Oculus frame");
    mFlags |= FLAG_OCULUS;
    auto ret
      = next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
    OculusFrameHook::Unhook();
    return ret;
  }

  void SetD3DFlags(IDXGISwapChain* swapChain) {
    dprint("Detected DXGI frame...");

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

  virtual HRESULT OnIDXGISwapChain_Present(
    UINT syncInterval,
    UINT flags,
    IDXGISwapChain* swapChain,
    const decltype(&IDXGISwapChain::Present)& next) override {
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

  virtual vr::EVRCompositorError OnIVRCompositor_Submit(
    vr::EVREye eEye,
    const vr::Texture_t* pTexture,
    const vr::VRTextureBounds_t* pBounds,
    vr::EVRSubmitFlags nSubmitFlags,
    vr::IVRCompositor* compositor,
    const decltype(&vr::IVRCompositor::Submit)& next) override {
    dprint("Detected SteamVR frame");
    mFlags |= FLAG_STEAMVR;
    IVRCompositorSubmitHook::Unhook();
    return std::invoke(next, compositor, eEye, pTexture, pBounds, nSubmitFlags);
  }

 private:
  void Next() {
    Unhook();

    if ((mFlags & FLAG_STEAMVR)) {
      dprint("Doing nothing as SteamVR is in-process");
      return;
    }

    if (mFlags == (FLAG_D3D11 | FLAG_OCULUS)) {
      LoadNext(RuntimeFiles::OCULUS_D3D11_DLL);
      return;
    }

    if (mFlags == FLAG_D3D11) {
      LoadNext(RuntimeFiles::NON_VR_D3D11_DLL);
      return;
    }

    dprintf(
      "Don't know how to create a kneeboard from autodetection flags {:#b}",
      mFlags);
  }

  void LoadNext(const std::filesystem::path& _next) {
    std::filesystem::path next(_next);
    if (!next.is_absolute()) {
      wchar_t buf[MAX_PATH];
      GetModuleFileNameW(mThisModule, buf, MAX_PATH);
      next = std::filesystem::path(buf).parent_path() / _next;
    }
    dprint("----- Loading next -----");
    dprintf("  Next: {}", next.string());
    auto wnext = next.wstring();
    if (!LoadLibraryW(wnext.c_str())) {
      dprintf("!!!!! Load failed: {:#x}", GetLastError());
    }
  }
};

}// namespace OpenKneeboard

namespace {

std::unique_ptr<AutoDetectKneeboard> gInstance;
HMODULE gModule = nullptr;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  DetourTransactionPushBegin();
  gInstance = std::make_unique<AutoDetectKneeboard>(gModule);
  DetourTransactionPopCommit();
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  gModule = hinst;
  return InjectedDLLMain(
    "OpenKneeboard-Autodetect",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
