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

// clang-format off
#include <shims/winrt/base.h>
#include <windows.h>
#include <psapi.h>
#include <d3d11.h>
#include <d3d12.h>
// clang-format on

#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>

#include <thread>

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
  IDXGISwapChainPresentHook mDXGIHook;

 public:
  InjectionBootstrapper() = delete;

  explicit InjectionBootstrapper(HMODULE self) : mThisModule(self) {
    wchar_t pathBuf[1024];
    const auto pathLen = GetModuleFileNameW(NULL, pathBuf, 1024);
    const auto executableDir = std::filesystem::canonical(
      std::filesystem::path({pathBuf, pathLen}).parent_path());
    for (const auto [module, path]: this->GetInProcessDLLs()) {
      if (path.parent_path() != executableDir) {
        continue;
      }

      const auto filename = path.filename().wstring();
      for (const auto hookDll: {L"d3d11.dll", L"dxgi.dll"}) {
        if (_wcsicmp(filename.c_str(), hookDll) == 0) {
          dprintf("Refusing to hook because found third-party dll: {}", path);
          return;
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
    CheckForOpenXRAPILayer();
    mPassthroughAll = true;

    // Whatever APIs are in use, if SteamVR is one of them, the main app will
    // use the OpenVR overlay API to render from that process, so we don't need
    // to do anything here.
    if ((mFlags & FLAG_STEAMVR)) {
      dprint("Doing nothing as SteamVR is in-process");
      return;
    }

    // Similarly, if the OpenKneeboard OpenXR API layer is loaded, that will
    // deal with the rendering.
    if ((mFlags & FLAG_OPENXR)) {
      dprint("Doing nothing as the OpenXR API layer is in-process.");
      return;
    }

    // Not looking for an exact match because mixing D3D11 and D3D12 is common
    if ((mFlags & FLAG_D3D12) && (mFlags & FLAG_OCULUS)) {
      LoadNext(RuntimeFiles::OCULUS_D3D12_DLL);
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
      "Don't know how to create a kneeboard from detection flags {:#b}",
      mFlags);
  }

  void CheckForOpenXRAPILayer() {
    std::vector<HMODULE> modules;
    DWORD bytesNeeded {};
    const auto process = GetCurrentProcess();
    EnumProcessModules(process, modules.data(), 0, &bytesNeeded);
    if (!bytesNeeded) {
      return;
    }
    modules.resize(bytesNeeded / sizeof(HMODULE));
    if (!EnumProcessModules(
          process, modules.data(), bytesNeeded, &bytesNeeded)) {
      dprintf(
        "Failed to get process module list: {}",
        static_cast<int64_t>(GetLastError()));
      return;
    }
    for (const auto module: modules) {
      wchar_t buffer[1024];
      const auto bufferLen
        = GetModuleFileNameW(module, buffer, std::size(buffer));
      if (!bufferLen) {
        continue;
      }
      const std::filesystem::path path {std::wstring_view {buffer, bufferLen}};
      if (path.filename() == RuntimeFiles::OPENXR_DLL) {
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

    dprintf("----- Loading next: {} -----", next.string());
    if (!LoadLibraryW(next.wstring().c_str())) {
      dprintf(
        "----- Load failed: {:#x} -----",
        std::bit_cast<uint32_t>(GetLastError()));
    }
  }
};

static std::unique_ptr<InjectionBootstrapper> gInstance;
static HMODULE gModule = nullptr;

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
    "OpenKneeboard-AutoDetect",
    gInstance,
    &ThreadEntry,
    hinst,
    dwReason,
    reserved);
}
