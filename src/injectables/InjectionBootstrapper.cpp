// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include "IDXGISwapChainHook.hpp"
#include "IVRCompositorWaitGetPosesHook.hpp"
#include "InjectedDLLMain.hpp"
#include "OculusEndFrameHook.hpp"

#include <OpenKneeboard/RuntimeFiles.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>
#include <Psapi.h>
#include <d3d11.h>
#include <d3d12.h>

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
class InjectionBootstrapper final {
 private:
  const uint64_t FLAG_D3D11 = 1 << 0;
  const uint64_t FLAG_D3D12 = 1 << 1;
  const uint64_t FLAG_OCULUS = 1ui64 << 32;
  const uint64_t FLAG_STEAMVR = 1ui64 << 33;
  const uint64_t FLAG_OPENXR = 1ui64 << 34;

  HMODULE mThisModule = nullptr;
  uint64_t mFlags = 0;
  uint64_t mFrames = 0;

  bool mPassthroughAll = false;
  bool mPassthroughOculus = false;
  bool mPassthroughDXGI = false;
  bool mPassthroughSteamVR = false;

  OculusEndFrameHook mOculusHook;
  IVRCompositorWaitGetPosesHook mOpenVRHook;
  IDXGISwapChainHook mDXGIHook;

 public:
  explicit InjectionBootstrapper(HMODULE self) : mThisModule(self) {
    wchar_t pathBuf[1024];
    const auto pathLen = GetModuleFileNameW(NULL, pathBuf, 1024);
    const auto executableDir = std::filesystem::canonical(
      std::filesystem::path({pathBuf, pathLen}).parent_path());
    const auto dlls = this->GetInProcessDLLs();
    const auto haveSteamOverlay
      = std::ranges::any_of(dlls, [](const auto& pair) {
          const auto path = std::get<1>(pair);
          return path.filename() == "GameOverlayRenderer64.dll";
        });
    for (const auto& [module, path]: dlls) {
      if (path.parent_path() != executableDir) {
        continue;
      }

      const auto filename = path.filename().wstring();
      for (const auto hookDll: {L"d3d11.dll", L"dxgi.dll"}) {
        if (_wcsicmp(filename.c_str(), hookDll) == 0) {
          if (haveSteamOverlay) {
            dprint("Found third-party dll: {}", path);
            dprint(
              "Ignoring because Steam overlay is present - can piggy-back");
          } else {
            dprint("Refusing to hook because found third-party dll: {}", path);
            return;
          }
        }
      }
    }

    mOculusHook.InstallHook({
      .onEndFrame
      = std::bind_front(&InjectionBootstrapper::OnOVREndFrame, this),
    });
    mOpenVRHook.InstallHook({
      .onWaitGetPoses = std::bind_front(
        &InjectionBootstrapper::OnIVRCompositor_WaitGetPoses, this),
    });
    mDXGIHook.InstallHook({
      .onPresent
      = std::bind_front(&InjectionBootstrapper::OnIDXGISwapChain_Present, this),
    });
  }

  ~InjectionBootstrapper() {
    mOculusHook.UninstallHook();
    mOpenVRHook.UninstallHook();
    mDXGIHook.UninstallHook();
  }

  InjectionBootstrapper() = delete;
  InjectionBootstrapper(const InjectionBootstrapper&) = delete;
  InjectionBootstrapper(InjectionBootstrapper&&) = delete;
  InjectionBootstrapper& operator=(const InjectionBootstrapper&) = delete;
  InjectionBootstrapper& operator=(InjectionBootstrapper&&) = delete;

 protected:
  ovrResult OnOVREndFrame(
    ovrSession session,
    long long frameIndex,
    const ovrViewScaleDesc* viewScaleDesc,
    ovrLayerHeader const* const* layerPtrList,
    unsigned int layerCount,
    const decltype(&ovr_EndFrame)& next) {
    if (!(mPassthroughAll || mPassthroughOculus)) [[unlikely]] {
      dprint("Detected Oculus frame");
      mFlags |= FLAG_OCULUS;
      mPassthroughOculus = true;
    }
    return next(session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
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

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* swapChain,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next) {
    if (mPassthroughAll || mPassthroughDXGI) [[likely]] {
      return std::invoke(next, swapChain, syncInterval, flags);
    }
    if (mFrames == 0) {
      dprint("Got first DXGI frame");
      SetD3DFlags(swapChain);
    }
    mFrames++;

    // Wait for anything else, e.g. SteamVR, Oculus, OpenVR
    if (mFrames >= 100) {
      mPassthroughDXGI = true;
      Next();
    }

    return std::invoke(next, swapChain, syncInterval, flags);
  }

  vr::EVRCompositorError OnIVRCompositor_WaitGetPoses(
    vr::IVRCompositor* compositor,
    vr::TrackedDevicePose_t* pRenderPoseArray,
    uint32_t unRenderPoseArrayCount,
    vr::TrackedDevicePose_t* pGamePoseArray,
    uint32_t unGamePoseArrayCount,
    const decltype(&vr::IVRCompositor::WaitGetPoses)& next) {
    if (!(mPassthroughAll || mPassthroughSteamVR)) [[unlikely]] {
      dprint("Detected SteamVR frame");
      mFlags |= FLAG_STEAMVR;
      mPassthroughSteamVR = true;
    }
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
    dprint("Going Next()");
    this->CheckForOpenXRAPILayer();
    mPassthroughAll = true;

    // Whatever APIs are in use, if SteamVR is one of them, the main app will
    // use the OpenVR overlay API to render from that process, so we don't need
    // to do anything here.
    if ((mFlags & FLAG_STEAMVR)) {
      dprint.Warning("Doing nothing as SteamVR is in-process");
      return;
    }

    // Similarly, if the OpenKneeboard OpenXR API layer is loaded, that will
    // deal with the rendering.
    if ((mFlags & FLAG_OPENXR)) {
      dprint.Warning("Doing nothing as the OpenXR API layer is in-process.");
      return;
    }

    // Not looking for an exact match because mixing D3D11 and D3D12 is common
    if ((mFlags & FLAG_D3D12) && (mFlags & FLAG_OCULUS)) {
      dprint.Warning("Detected Oculus+D3D12, which is no longer supported");
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

    dprint.Warning(
      "Don't know how to create a kneeboard from detection flags {:#b}",
      mFlags);
  }

  std::vector<std::tuple<HMODULE, std::filesystem::path>> GetInProcessDLLs() {
    std::vector<HMODULE> modules;
    DWORD bytesNeeded {};
    const auto process = GetCurrentProcess();
    EnumProcessModules(process, modules.data(), 0, &bytesNeeded);
    if (!bytesNeeded) {
      return {};
    }
    modules.resize(bytesNeeded / sizeof(HMODULE));
    if (!EnumProcessModules(
          process, modules.data(), bytesNeeded, &bytesNeeded)) {
      dprint(
        "Failed to get process module list: {}",
        static_cast<int64_t>(GetLastError()));
      return {};
    }

    std::vector<std::tuple<HMODULE, std::filesystem::path>> ret;
    for (const auto module: modules) {
      wchar_t buffer[1024];
      const auto bufferLen
        = GetModuleFileNameW(module, buffer, std::size(buffer));
      if (!bufferLen) {
        continue;
      }
      const std::filesystem::path path {std::wstring_view {buffer, bufferLen}};
      if (std::filesystem::exists(path)) {
        ret.push_back({module, std::filesystem::canonical(path)});
      }
    }
    return ret;
  }

  void CheckForOpenXRAPILayer() {
    for (auto [module, path]: GetInProcessDLLs()) {
      if (
        path.filename() == RuntimeFiles::OPENXR_64BIT_DLL
        || path.filename() == RuntimeFiles::OPENXR_32BIT_DLL) {
        dprint("Found OpenKneeboard OpenXR API layer in-process");
        mFlags |= FLAG_OPENXR;
        return;
      }
    }
  }

  void LoadNext(const std::filesystem::path& _next) {
    std::filesystem::path next(_next);
    if (!next.is_absolute()) {
      wchar_t buf[MAX_PATH];
      GetModuleFileNameW(mThisModule, buf, MAX_PATH);
      next = std::filesystem::path(buf).parent_path() / _next;
    }

    dprint("----- Loading next: {} -----", next.string());
    if (!LoadLibraryW(next.wstring().c_str())) {
      dprint(
        "----- Load failed: {:#x} -----",
        std::bit_cast<uint32_t>(GetLastError()));
    }
  }
};

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.AutoDetect")
 * cb1c6ba7-9801-5736-cc7c-c37fcca3feb7
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.AutoDetect",
  (0xcb1c6ba7, 0x9801, 0x5736, 0xcc, 0x7c, 0xc3, 0x7f, 0xcc, 0xa3, 0xfe, 0xb7));

}// namespace OpenKneeboard

namespace {

std::unique_ptr<InjectionBootstrapper> gInstance;
HMODULE gModule = nullptr;

DWORD WINAPI ThreadEntry(LPVOID ignored) {
  gInstance = std::make_unique<InjectionBootstrapper>(gModule);
  dprint("Installed hooks.");

  return S_OK;
}

}// namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  gModule = hinst;
  return InjectedDLLMain(
    "OpenKneeboard-AutoDetect",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
