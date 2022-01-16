
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

/** Figure out which kneeboard a process wants.
 *
 * Hook into various APIs, wait to see if they're used, and once we have enough
 * frames, load a concrete kneeboard implementation and unload itself.
 * 
 * For example, if SteamVR is used, don't load anything. If only Oculus and
 * D3D11 are used, load OpenKneeboard-oculus-d3d11.dll. "ONly D3D11" in case
 * 11on12 is used.
 */
class InjectionBootstrapper final : private OculusEndFrameHook,
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
  InjectionBootstrapper() = delete;

  explicit InjectionBootstrapper(HMODULE self) : mThisModule(self) {
    OculusEndFrameHook::InitWithVTable();
    IDXGISwapChainPresentHook::InitWithVTable();
    IVRCompositorWaitGetPosesHook::InitWithVTable();
  }

  ~InjectionBootstrapper() {
    this->UninstallHook();
  }

  void UninstallHook() {
    OculusEndFrameHook::UninstallHook();
    IDXGISwapChainPresentHook::UninstallHook();
    IVRCompositorWaitGetPosesHook::UninstallHook();
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
    OculusEndFrameHook::UninstallHook();
    return ret;
  }

  void SetD3DFlags(IDXGISwapChain* swapChain) {
    dprint("Detected DXGI frame...");

    winrt::com_ptr<IUnknown> device;
    swapChain->GetDevice(IID_PPV_ARGS(device.put()));

    if (device.try_as<ID3D11Device>()) {
      dprint("... found D3D11");
      mFlags |= FLAG_D3D11;
      return;
    }

    if (device.try_as<ID3D12Device>()) {
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
      IDXGISwapChainPresentHook::UninstallHook();
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
    IVRCompositorWaitGetPosesHook::UninstallHook();
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
    UninstallHook();

    if ((mFlags & FLAG_STEAMVR)) {
      dprint("Doing nothing as SteamVR is in-process");
      UnloadWithoutNext();
      return;
    }

    if (mFlags == (FLAG_D3D11 | FLAG_OCULUS)) {
      LoadNextThenUnload(RuntimeFiles::OCULUS_D3D11_DLL);
      return;
    }

    if (mFlags == FLAG_D3D11) {
      LoadNextThenUnload(RuntimeFiles::NON_VR_D3D11_DLL);
      return;
    }

    dprintf(
      "Don't know how to create a kneeboard from detection flags {:#b}",
      mFlags);
    UnloadWithoutNext();
  }

  static DWORD WINAPI LoadNextThenUnloadThreadImpl(void* data);
  static DWORD WINAPI UnloadWithoutNextThreadImpl(void* data);

  void UnloadWithoutNext() {
    this->UninstallHook();

    CreateThread(
      nullptr,
      0,
      &UnloadWithoutNextThreadImpl,
      nullptr,
      0,
      nullptr);
  }

  void LoadNextThenUnload(const std::filesystem::path& _next) {
    this->UninstallHook();

    std::filesystem::path next(_next);
    if (!next.is_absolute()) {
      wchar_t buf[MAX_PATH];
      GetModuleFileNameW(mThisModule, buf, MAX_PATH);
      next = std::filesystem::path(buf).parent_path() / _next;
    }

    CreateThread(
      nullptr,
      0,
      &LoadNextThenUnloadThreadImpl,
      new std::filesystem::path(next),
      0,
      nullptr);
  }
};

static std::unique_ptr<InjectionBootstrapper> gInstance;
static HMODULE gModule = nullptr;

static void CleanupHookInstance() {
  if (gInstance) {
    dprint("----- Cleaning up bootstrapper instance -----");
    gInstance.reset();
  }
}

DWORD InjectionBootstrapper::UnloadWithoutNextThreadImpl(void* unused) {
  CleanupHookInstance();

  dprintf("----- Freeing bootstrapper DLL ----");
  FreeLibraryAndExitThread(gModule, S_OK);

  return S_OK;
}

DWORD InjectionBootstrapper::LoadNextThenUnloadThreadImpl(void* data) {
  CleanupHookInstance();

  auto path = reinterpret_cast<std::filesystem::path*>(data);
  dprintf("----- Loading next: {} -----", path->string());
  if (!LoadLibraryW(path->c_str())) {
    dprintf("----- Load failed: {:#x} -----", GetLastError());
  }
  delete path;

  return UnloadWithoutNextThreadImpl(nullptr);
}

}// namespace OpenKneeboard

namespace {

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<InjectionBootstrapper>(gModule);
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  gModule = hinst;
  return InjectedDLLMain(
    "OpenKneeboard-InjectionBootstrapper",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
