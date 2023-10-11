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
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/TabletInputAdapter.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/weak_wrap.h>

#include <shims/utility>
#include <shims/winrt/base.h>

#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_set>

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

}// namespace

namespace OpenKneeboard {

std::shared_ptr<GameInjector> GameInjector::Create(KneeboardState* state) {
  return std::shared_ptr<GameInjector>(new GameInjector(state));
}

GameInjector::GameInjector(KneeboardState* state) : mKneeboardState(state) {
  const auto dllPath
    = std::filesystem::canonical(RuntimeFiles::GetInstallationDirectory());
  mTabletProxyDll = dllPath / RuntimeFiles::TABLET_PROXY_DLL;
  mOverlayAutoDetectDll = dllPath / RuntimeFiles::AUTODETECTION_DLL;
  mOverlayNonVRD3D11Dll = dllPath / RuntimeFiles::NON_VR_D3D11_DLL;
  mOverlayOculusD3D11Dll = dllPath / RuntimeFiles::OCULUS_D3D11_DLL;
  mOverlayOculusD3D12Dll = dllPath / RuntimeFiles::OCULUS_D3D12_DLL;
}

GameInjector::~GameInjector() {
  RemoveAllEventListeners();
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

  this->RemoveEventListener(mTabletSettingsChangeToken);
  auto tablet = mKneeboardState->GetTabletInputAdapter();
  if (tablet) {
    mWintabMode = tablet->GetWintabMode();
    mTabletSettingsChangeToken = AddEventListener(
      tablet->evSettingsChangedEvent,
      weak_wrap(this, tablet)([](auto self, auto tablet) {
        self->mWintabMode = tablet->GetWintabMode();
      }));
  } else {
    mWintabMode = WintabMode::Disabled;
  }

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

    std::unordered_set<DWORD> seenProcesses;
    do {
      seenProcesses.insert(process.th32ProcessID);
      CheckProcess(process.th32ProcessID, process.szExeFile);
    } while (Process32Next(snapshot.get(), &process));

    auto it = mProcessCache.begin();
    while (it != mProcessCache.end()) {
      if (seenProcesses.contains(it->first)) {
        ++it;
        continue;
      }
      it = mProcessCache.erase(it);
    }

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
        dprintf(
          L"Failed to OpenProcess() for PID {} ({}): {:#x}",
          processID,
          exeBaseName,
          std::bit_cast<uint32_t>(GetLastError()));
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

    InjectedDlls wantedDlls {InjectedDlls::None};

    if (mWintabMode == WintabMode::EnabledInvasive) {
      wantedDlls |= InjectedDlls::TabletProxy;
    }

    std::filesystem::path overlayDll;
    switch (game->mOverlayAPI) {
      case OverlayAPI::None:
      case OverlayAPI::SteamVR:
      case OverlayAPI::OpenXR:
        break;
      case OverlayAPI::AutoDetect:
        wantedDlls |= InjectedDlls::AutoDetection;
        break;
      case OverlayAPI::NonVRD3D11:
        wantedDlls |= InjectedDlls::NonVRD3D11;
        break;
      case OverlayAPI::OculusD3D11:
        wantedDlls |= InjectedDlls::OculusD3D11;
        break;
      case OverlayAPI::OculusD3D12:
        wantedDlls |= InjectedDlls::OculusD3D12;
        break;
      default:
        dprintf(
          "Unhandled OverlayAPI: {}", std23::to_underlying(game->mOverlayAPI));
        OPENKNEEBOARD_BREAK;
        return;
    }

    if (!mProcessCache.contains(processID)) {
      mProcessCache.insert_or_assign(processID, InjectedDlls::None);
      // Lazy to-string approach
      nlohmann::json overlayAPI;
      to_json(overlayAPI, game->mOverlayAPI);

      dprintf(
        "Current game changed to {}, PID {}, configured rendering API {}",
        game->mPath.string(),
        processID,
        overlayAPI.dump());
      this->evGameChangedEvent.Emit(processID, game);
    }

    auto& currentDlls = mProcessCache.at(processID);
    const auto missingDlls = wantedDlls & ~currentDlls;
    if (missingDlls == InjectedDlls::None) {
      continue;
    }

    dprintf("Injecting DLLs into PID {} ({})", processID, fullPath.string());

    const auto injectIfNeeded = [&](const auto dllID, const auto& dllPath) {
      if (!static_cast<bool>(missingDlls & dllID)) {
        return;
      }
      if (IsInjected(processHandle.get(), dllPath)) {
        currentDlls |= dllID;
        return;
      }
      InjectDll(processHandle.get(), dllPath);
      currentDlls |= dllID;
    };

    injectIfNeeded(InjectedDlls::TabletProxy, mTabletProxyDll);
    injectIfNeeded(InjectedDlls::AutoDetection, mOverlayAutoDetectDll);
    injectIfNeeded(InjectedDlls::NonVRD3D11, mOverlayNonVRD3D11Dll);
    injectIfNeeded(InjectedDlls::OculusD3D11, mOverlayOculusD3D11Dll);
    injectIfNeeded(InjectedDlls::OculusD3D12, mOverlayOculusD3D12Dll);
  }
}

// This function assumes that the dll was injected with the path
// in the same form as is being checked
bool GameInjector::IsInjected(
  HANDLE process,
  const std::filesystem::path& dll) {
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

  wchar_t buf[MAX_PATH];
  for (auto module: modules) {
    auto length = GetModuleFileNameExW(process, module, buf, MAX_PATH);
    if (std::wstring_view(buf, length) == dll) {
      return true;
    }
  }

  return false;
}

bool GameInjector::InjectDll(HANDLE process, const std::filesystem::path& dll) {
  if (IsInjected(process, dll)) {
    return false;
  }

  DebugPrivileges privileges;

  auto dllStr = dll.wstring();

  const auto dllStrByteLen = (dllStr.size() + 1) * sizeof(wchar_t);
  auto targetBuffer = VirtualAllocEx(
    process, nullptr, dllStrByteLen, MEM_COMMIT, PAGE_READWRITE);
  if (!targetBuffer) {
    dprint("Failed to VirtualAllocEx");
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
    return false;
  }

  WaitForSingleObject(thread.get(), INFINITE);
  // We EnumProcessModules again as thread exit code is a DWORD, and
  // sizeof(DWORD) < sizeof(HMODULE)
  if (!IsInjected(process, dll)) {
    dprintf("Injecting {} failed :'(", dll.string());
    return false;
  }

  dprintf("Injected {}", dll.string());
  return true;
}

}// namespace OpenKneeboard
