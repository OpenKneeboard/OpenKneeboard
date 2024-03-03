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
#include <Shlwapi.h>
#include <Psapi.h>
#include <appmodel.h>
#include <WtsApi32.h>
// clang-format on

#include <OpenKneeboard/Elevation.h>
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

winrt::Windows::Foundation::IAsyncAction GameInjector::Run(
  std::stop_token stopToken) {
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
  do {
    co_await resume_after(stopToken, std::chrono::milliseconds(200));
    if (stopToken.stop_requested()) {
      co_return;
    }
    OPENKNEEBOARD_TraceLoggingScope("GameInjector::Run()/EnumerateProcesses");
    WTS_PROCESS_INFO_EXW* processes {nullptr};
    DWORD processCount {0};
    DWORD level = 1;
    {
      OPENKNEEBOARD_TraceLoggingScope("WTSEnumerateProcessesExW()");
      if (!WTSEnumerateProcessesExW(
            WTS_CURRENT_SERVER_HANDLE,
            &level,
            WTS_CURRENT_SESSION,
            reinterpret_cast<LPWSTR*>(&processes),
            &processCount)) {
        const auto code = GetLastError();
        if (code == ERROR_BAD_LENGTH) {
          continue;// ?! We're not providing a length...
        }
        OPENKNEEBOARD_LOG_AND_FATAL(
          "WTSEnumerateProcessesExW() failed with {}", code);
      }
    }

    std::unordered_set<DWORD> seenProcesses;
    for (int i = 0; i < processCount; ++i) {
      const auto& process = processes[i];
      seenProcesses.insert(process.ProcessId);
      if (process.pUserSid == 0) {
        continue;
      }
      CheckProcess(process.ProcessId, process.pProcessName);
    }
    WTSFreeMemory(processes);
    processes = nullptr;

    auto it = mProcessCache.begin();
    while (it != mProcessCache.end()) {
      if (seenProcesses.contains(it->first)) {
        ++it;
        continue;
      }
      it = mProcessCache.erase(it);
    }
  } while (!stopToken.stop_requested());
}

void GameInjector::CheckProcess(
  DWORD processID,
  std::wstring_view exeBaseName) {
  HANDLE processHandle {NULL};
  std::filesystem::path fullPath;

  std::scoped_lock lock(mGamesMutex);
  for (const auto& game: mGames) {
    if (game->mLastSeenPath.filename() != exeBaseName) {
      continue;
    }
    if (!processHandle) {
      if (mProcessCache.contains(processID)) {
        const auto& entry = mProcessCache.at(processID);
        processHandle = entry.mHandle.get();
        fullPath = entry.mPath;
      } else {
        processHandle
          = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processID);
        if (processHandle) {
          wchar_t buf[MAX_PATH];
          DWORD bufSize = sizeof(buf);
          if (
            QueryFullProcessImageNameW(processHandle, 0, buf, &bufSize)
            && bufSize >= 0 && bufSize <= sizeof(buf)) {
            fullPath
              = std::filesystem::canonical(std::wstring_view {buf, bufSize});
          }
          mProcessCache.emplace(
            processID,
            ProcessCacheEntry {
              .mHandle = winrt::handle {processHandle},
              .mPath = fullPath,
            });
        }
      }
    }
    if (!processHandle) {
      dprintf(
        L"Failed to OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION) for PID {} "
        L"({}): {:#x}",
        processID,
        exeBaseName,
        std::bit_cast<uint32_t>(GetLastError()));
      return;
    }

    if (
      PathMatchSpecExW(
        fullPath.wstring().c_str(),
        winrt::to_hstring(game->mPathPattern).c_str(),
        PMSF_NORMAL | PMSF_DONT_STRIP_SPACES)
      != S_OK) {
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

    const auto currentGame = mKneeboardState->GetCurrentGame();
    const DWORD currentPID = currentGame ? currentGame->mProcessID : 0;

    if (currentPID != processID) {
      // Lazy to-string approach
      nlohmann::json overlayAPI;
      to_json(overlayAPI, game->mOverlayAPI);

      dprintf(
        "Current game changed to {}, PID {}, configured rendering API {}",
        fullPath.string(),
        processID,
        overlayAPI.dump());
      const auto elevated = IsElevated(processHandle);
      if (IsElevated() != elevated) {
        dprintf(
          "WARNING: OpenKneeboard {} elevated, but PID {} {} elevated.",
          IsElevated() ? "is" : "is not",
          processID,
          elevated ? "is" : "is not");
      }
      this->evGameChangedEvent.Emit(processID, fullPath, game);
    }

    auto& process = mProcessCache.at(processID);

    using InjectionAccessState = ProcessCacheEntry::InjectionAccessState;
    if (process.mInjectionAccessState == InjectionAccessState::Failed) {
      continue;
    }

    auto& currentDlls = process.mInjectedDlls;
    const auto missingDlls = wantedDlls & ~currentDlls;
    if (missingDlls == InjectedDlls::None && process.mHaveLoggedDLLs) {
      continue;
    }

    if (process.mInjectionAccessState == InjectionAccessState::NotTried) {
      dprintf("Reopening PID {} with VM and thread privileges", processID);
      processHandle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ
          | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD,
        false,
        processID);
      if (!processHandle) {
        const auto code = GetLastError();
        const auto message
          = std::system_category().default_error_condition(code).message();
        dprintf(
          "ERROR: Failed to OpenProcess() with VM and thread privileges for "
          "PID {} ({}): "
          "{:#x} ({})",
          processID,
          winrt::to_string(exeBaseName),
          std::bit_cast<uint32_t>(code),
          message);
        process.mInjectionAccessState = InjectionAccessState::Failed;
        continue;
      }
      process.mHandle = winrt::handle {processHandle};
      process.mInjectionAccessState = InjectionAccessState::HaveInjectionAccess;
      dprint("Reopened with VM and thread privileges");
    }

    const auto dlls = GetProcessCurrentDLLs(processHandle);
    if (!process.mHaveLoggedDLLs) {
      process.mHaveLoggedDLLs = true;
      if (dlls.empty()) {
        dprint("Failed to get DLL list");
        continue;
      }
      std::vector<std::filesystem::path> sortedDLLs {dlls.begin(), dlls.end()};
      std::ranges::sort(sortedDLLs);
      for (const auto& dll: sortedDLLs) {
        dprintf(
          L"{} (PID {}) module: {}", exeBaseName, processID, dll.wstring());
      }
    }

    if (dlls.empty()) {
      continue;
    }

    if (missingDlls == InjectedDlls::None) {
      continue;
    }
    dprintf("Injecting DLLs into PID {} ({})", processID, fullPath.string());

    const auto injectIfNeeded = [&](const auto dllID, const auto& dllPath) {
      if (!static_cast<bool>(missingDlls & dllID)) {
        return;
      }
      if (dlls.contains(dllPath)) {
        currentDlls |= dllID;
        dprintf("{} is already injected", dllPath.filename().string());
        return;
      }
      InjectDll(processHandle, dllPath);
      currentDlls |= dllID;
    };

    injectIfNeeded(InjectedDlls::TabletProxy, mTabletProxyDll);
    injectIfNeeded(InjectedDlls::AutoDetection, mOverlayAutoDetectDll);
    injectIfNeeded(InjectedDlls::NonVRD3D11, mOverlayNonVRD3D11Dll);
    injectIfNeeded(InjectedDlls::OculusD3D11, mOverlayOculusD3D11Dll);
    injectIfNeeded(InjectedDlls::OculusD3D12, mOverlayOculusD3D12Dll);
  }
}

std::unordered_set<std::filesystem::path> GameInjector::GetProcessCurrentDLLs(
  HANDLE process) {
  std::filesystem::path executablePath;
  DWORD neededBytes = 0;
  EnumProcessModules(process, nullptr, 0, &neededBytes);
  std::vector<HMODULE> modules;
  constexpr int maxTries = 5;
  for (int i = 0; i < maxTries; ++i) {
    modules.resize(neededBytes / sizeof(HMODULE));
    DWORD requestedBytes = neededBytes;
    if (EnumProcessModules(
          process, modules.data(), requestedBytes, &neededBytes)) {
      if (neededBytes < requestedBytes) {
        modules.resize(neededBytes / sizeof(HMODULE));
      }
      break;
    }
    const auto code = GetLastError();
    dprintf(
      "EnumProcessModules() failed: {:#x} ({})",
      std::bit_cast<uint32_t>(code),
      std::system_category().default_error_condition(code).message());
    if (code != ERROR_PARTIAL_COPY) {
      break;
    }
    if (i == maxTries - 1) {
      dprintf("Failed to EnumProcessModules {} times, giving up", maxTries);
      OPENKNEEBOARD_BREAK;
      return {};
    }
  }

  std::unordered_set<std::filesystem::path> ret;
  wchar_t buf[MAX_PATH];
  for (const auto module: modules) {
    const auto length = GetModuleFileNameExW(process, module, buf, MAX_PATH);
    if (length == 0) {
      continue;
    }
    std::filesystem::path path {std::wstring_view {buf, length}};
    if (path.is_absolute()) {
      try {
        ret.emplace(std::filesystem::weakly_canonical(path));
      } catch (const std::filesystem::filesystem_error&) {
      }
      continue;
    }
    if (executablePath.empty()) {
      const auto length = GetProcessImageFileNameW(process, buf, MAX_PATH);
      if (length == 0) {
        continue;
      }
      try {
        executablePath
          = std::filesystem::weakly_canonical(std::wstring_view {buf, length});
      } catch (const std::filesystem::filesystem_error&) {
        continue;
      }
    }
    path = executablePath.parent_path() / path;
    if (std::filesystem::exists(path)) {
      try {
        ret.emplace(std::filesystem::weakly_canonical(path));
      } catch (const std::filesystem::filesystem_error&) {
      }
    }
  }
  return ret;
}

bool GameInjector::InjectDll(HANDLE process, const std::filesystem::path& dll) {
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
  // We fetch the full list again as thread exit code is a DWORD, and
  // sizeof(DWORD) < sizeof(HMODULE)
  if (!GetProcessCurrentDLLs(process).contains(dll)) {
    dprintf("DLL {} not found in list after injection :'(", dll.string());
    return false;
  }

  dprintf("Injected {}", dll.string());
  return true;
}

}// namespace OpenKneeboard
