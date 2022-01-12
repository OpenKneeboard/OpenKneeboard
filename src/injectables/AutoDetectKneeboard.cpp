
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>
#include <Unknwn.h>
#include <d3d11.h>
#include <d3d12.h>
#include <windows.h>
#include <winrt/base.h>

#include "IDXGISwapChainPresentHook.h"
#include "IVRCompositorWaitGetPosesHook.h"
#include "InjectedDLLMain.h"
#include "OculusEndFrameHook.h"
#include "detours-ext.h"

using namespace OpenKneeboard;

namespace OpenKneeboard {

class AutoDetectKneeboard final : private OculusEndFrameHook,
                                  private IDXGISwapChainPresentHook,
                                  private IVRCompositorWaitGetPosesHook {
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
    OculusEndFrameHook::Unhook();
    IDXGISwapChainPresentHook::Unhook();
    IVRCompositorWaitGetPosesHook::Unhook();
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
    OculusEndFrameHook::Unhook();
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
    IDXGISwapChain* swapChain,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next) override {
    if (mFrames == 0) {
      SetD3DFlags(swapChain);
    }
    mFrames++;

    // Wait for anything else, e.g. SteamVR, Oculus, OpenVR
    if (mFrames >= 30) {
      IDXGISwapChainPresentHook::UnhookAndCleanup();
      Next();
    }

    return std::invoke(next, swapChain, syncInterval, flags);
  }

  virtual vr::EVRCompositorError OnIVRCompositor_WaitGetPoses(
    vr::IVRCompositor* compositor,
    vr::TrackedDevicePose_t* pRenderPoseArray,
    uint32_t unRenderPoseArrayCount,
    vr::TrackedDevicePose_t* pGamePoseArray,
    uint32_t unGamePoseArrayCount,
    const decltype(&vr::IVRCompositor::WaitGetPoses)& next) override {
    dprint("Detected SteamVR frame");
    mFlags |= FLAG_STEAMVR;
    IVRCompositorWaitGetPosesHook::Unhook();
    return std::invoke(
      next,
      compositor,
      pRenderPoseArray,
      unRenderPoseArrayCount,
      pGamePoseArray,
      unGamePoseArrayCount);
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
  gInstance = std::make_unique<AutoDetectKneeboard>(gModule);
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
