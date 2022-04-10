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
#include <Windows.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <Psapi.h>
#include <appmodel.h>
// clang-format on

#include <OpenKneeboard/GameInjector.h>
#include <OpenKneeboard/GameInstance.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/version.h>
#include <shims/winrt.h>

#include <chrono>
#include <mutex>
#include <thread>

namespace {

using namespace OpenKneeboard;

class DebugPrivileges final {
 private:
  winrt::handle mToken;
  LUID mLuid;

 public:
  DebugPrivileges() {
    if (!OpenProcessToken(
          GetCurrentProcess(),
          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
          mToken.put())) {
      dprint("Failed to open own process token");
      return;
    }

    LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &mLuid);

    TOKEN_PRIVILEGES privileges;
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = mLuid;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(
      mToken.get(), false, &privileges, sizeof(privileges), nullptr, nullptr);
  }

  ~DebugPrivileges() {
    if (!mToken) {
      return;
    }
    TOKEN_PRIVILEGES privileges;
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = mLuid;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_REMOVED;
    AdjustTokenPrivileges(
      mToken.get(), false, &privileges, sizeof(privileges), nullptr, nullptr);
  }
};

bool AlreadyInjected(HANDLE process, const std::filesystem::path& dll) {
  DWORD neededBytes = 0;
  EnumProcessModules(process, nullptr, 0, &neededBytes);
  std::vector<HMODULE> modules(neededBytes / sizeof(HMODULE));
  DWORD requestedBytes = neededBytes;
  if (!EnumProcessModules(
        process, modules.data(), requestedBytes, &neededBytes)) {
    // Maybe a lie, but if we can't list modules, we definitely can't inject one
    return true;
  }
  if (neededBytes < requestedBytes) {
    modules.resize(neededBytes / sizeof(HMODULE));
  }

  auto dllBaseName = dll.filename().wstring();

  wchar_t buf[MAX_PATH];
  for (auto module: modules) {
    auto length = GetModuleBaseNameW(process, module, buf, MAX_PATH);
    if (std::wstring_view(buf, length) == dllBaseName) {
      return true;
    }
  }

  return false;
}

bool InjectDll(HANDLE process, const std::filesystem::path& _dll) {
  const auto dll = std::filesystem::canonical(_dll);
  if (AlreadyInjected(process, dll)) {
    dprintf(
      "Asked to load a DLL ({}) that's already loaded", dll.stem().string());
    return false;
  }

  DebugPrivileges privileges;

  auto dllStr = dll.wstring();

  const auto dllStrByteLen = (dllStr.size() + 1) * sizeof(wchar_t);
  auto targetBuffer = VirtualAllocEx(
    process, nullptr, dllStrByteLen, MEM_COMMIT, PAGE_READWRITE);
  if (!targetBuffer) {
    return false;
  }

  const auto cleanup
    = scope_guard([&]() { VirtualFree(targetBuffer, 0, MEM_RELEASE); });
  WriteProcessMemory(
    process, targetBuffer, dllStr.c_str(), dllStrByteLen, nullptr);

  auto kernel32 = GetModuleHandleA("Kernel32");
  if (!kernel32) {
    dprint("Failed to open kernel32");
    return false;
  }

  auto loadLibraryW = reinterpret_cast<PTHREAD_START_ROUTINE>(
    GetProcAddress(kernel32, "LoadLibraryW"));
  if (!loadLibraryW) {
    dprint("Failed to find loadLibraryW");
    return false;
  }

  winrt::handle thread {CreateRemoteThread(
    process, nullptr, 0, loadLibraryW, targetBuffer, 0, nullptr)};

  if (!thread) {
    dprint("Failed to create remote thread");
  }

  WaitForSingleObject(thread.get(), INFINITE);
  DWORD loadLibraryReturn;
  GetExitCodeThread(thread.get(), &loadLibraryReturn);
  if (loadLibraryReturn == 0) {
    dprintf("Injecting {} failed :'(", dll.string());
  } else {
    dprintf("Injected {}", dll.string());
  }
  return true;
}

bool HaveWintab() {
  // Don't bother installing wintab support if the user doesn't have any
  // wintab drivers installed
  auto wintab = LoadLibraryA("Wintab32.dll");
  FreeLibrary(wintab);
  return (bool)wintab;
}

}// namespace

namespace OpenKneeboard {

GameInjector::GameInjector() {
  const auto dllPath = RuntimeFiles::GetDirectory();
  mMarkerDll = dllPath / RuntimeFiles::AUTOINJECT_MARKER_DLL;
  mTabletProxyDll = dllPath / RuntimeFiles::TABLET_PROXY_DLL;

  mOverlayAutoDetectDll = dllPath / RuntimeFiles::INJECTION_BOOTSTRAPPER_DLL;
  mOverlayNonVRD3D11Dll = dllPath / RuntimeFiles::NON_VR_D3D11_DLL;
  mOverlayOculusD3D11Dll = dllPath / RuntimeFiles::OCULUS_D3D11_DLL;
  mOverlayOculusD3D12Dll = dllPath / RuntimeFiles::OCULUS_D3D12_DLL;
}

void GameInjector::SetGameInstances(
  const std::vector<std::shared_ptr<GameInstance>>& games) {
  std::scoped_lock lock(mGamesMutex);
  mGames = games;
}

bool GameInjector::Run(std::stop_token stopToken) {
  PROCESSENTRY32 process;
  process.dwSize = sizeof(process);
  MODULEENTRY32 module;
  module.dwSize = sizeof(module);
  dprint("Watching for game processes");
  while (!stopToken.stop_requested()) {
    winrt::handle snapshot {CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
      dprintf("CreateToolhelp32Snapshot failed in {}", __FILE__);
      return false;
    }

    if (!Process32First(snapshot.get(), &process)) {
      dprintf("Process32First failed: {}", GetLastError());
      return false;
    }

    do {
      CheckProcess(process.th32ProcessID, process.szExeFile);
    } while (Process32Next(snapshot.get(), &process));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return true;
}

void GameInjector::CheckProcess(
  DWORD processID,
  std::wstring_view exeBaseName) {
  winrt::handle processHandle;
  std::filesystem::path fullPath;

  std::scoped_lock lock(mGamesMutex);
  for (const auto& game: mGames) {
    if (game->mPath.filename() != exeBaseName) {
      continue;
    }

    if (!processHandle) {
      processHandle.attach(OpenProcess(PROCESS_ALL_ACCESS, false, processID));
      if (!processHandle) {
        return;
      }

      wchar_t buf[MAX_PATH];
      DWORD bufSize = sizeof(buf);
      if (!QueryFullProcessImageNameW(processHandle.get(), 0, buf, &bufSize)) {
        return;
      }
      if (bufSize < 0 || bufSize > sizeof(buf)) {
        return;
      }
      fullPath = std::filesystem::canonical(std::wstring_view(buf, bufSize));
    }

    if (fullPath != game->mPath) {
      continue;
    }

    const auto friendly = game->mGame->GetUserFriendlyName(fullPath);

    if (AlreadyInjected(processHandle.get(), mMarkerDll)) {
      break;
    }

    dprintf("Found '{}' - PID {}", friendly, processID);
    InjectDll(processHandle.get(), mMarkerDll);

    if (HaveWintab()) {
      InjectDll(processHandle.get(), mTabletProxyDll);
    }

    std::filesystem::path overlayDll;
    switch (game->mOverlayAPI) {
      case OverlayAPI::SteamVR:
        dprint("SteamVR forced, not injecting any overlay DLL");
        return;
      case OverlayAPI::OpenXR:
        dprint("OpenXR forced, not injecting any overlay DLL");
        return;
      case OverlayAPI::AutoDetect:
        overlayDll = mOverlayAutoDetectDll;
        break;
      case OverlayAPI::NonVRD3D11:
        overlayDll = mOverlayNonVRD3D11Dll;
        break;
      case OverlayAPI::OculusD3D11:
        overlayDll = mOverlayOculusD3D11Dll;
        break;
      case OverlayAPI::OculusD3D12:
        overlayDll = mOverlayOculusD3D12Dll;
        break;
      default:
        dprintf(
          "Unhandled OverlayAPI: {}",
          static_cast<std::underlying_type_t<OverlayAPI>>(game->mOverlayAPI));
        OPENKNEEBOARD_BREAK;
        return;
    }

    if (overlayDll.empty() && std::filesystem::is_regular_file(overlayDll)) {
      dprintf(
        "No DLL for OverlayAPI value: {}",
        static_cast<std::underlying_type_t<OverlayAPI>>(game->mOverlayAPI));
      OPENKNEEBOARD_BREAK;
      continue;
    }

    if (!InjectDll(processHandle.get(), overlayDll)) {
      dprintf(
        "Failed to inject DLL {}: {}", overlayDll.string(), GetLastError());
      return;
    }
  }
}

}// namespace OpenKneeboard
