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
#include "okGameInjectorThread.h"

#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <shims/winrt.h>

#include <mutex>

#include "okEvents.h"
#include "scope_guard.h"

using namespace OpenKneeboard;

class okGameInjectorThread::Impl final {
 public:
  wxEvtHandler* mReceiver;
  std::vector<GameInstance> mGames;
  std::mutex mGamesMutex;
};

okGameInjectorThread::okGameInjectorThread(
  wxEvtHandler* receiver,
  const std::vector<GameInstance>& games) {
  p.reset(new Impl {
    .mReceiver = receiver,
    .mGames = games,
  });
}

okGameInjectorThread::~okGameInjectorThread() {
}

void okGameInjectorThread::SetGameInstances(
  const std::vector<GameInstance>& games) {
  std::scoped_lock lock(p->mGamesMutex);
  p->mGames = games;
}

namespace {
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

bool AlreadyInjected(HANDLE process, const std::filesystem::path& _dll) {
  std::filesystem::path dll = std::filesystem::canonical(_dll);
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
    if (std::filesystem::canonical(std::wstring_view(buf, length)) == dll) {
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

wxThread::ExitCode okGameInjectorThread::Entry() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(NULL, buf, MAX_PATH);
  const auto executablePath
    = std::filesystem::canonical(std::filesystem::path(buf).parent_path());
  const auto injectedDll
    = executablePath / RuntimeFiles::INJECTION_BOOTSTRAPPER_DLL;
  const auto markerDll = executablePath / RuntimeFiles::AUTOINJECT_MARKER_DLL;
  const auto tabletProxyDll = executablePath / RuntimeFiles::TABLET_PROXY_DLL;

  const auto installTabletProxy = HaveWintab();

  PROCESSENTRY32 process;
  process.dwSize = sizeof(process);
  MODULEENTRY32 module;
  module.dwSize = sizeof(module);
  dprint("Looking for game processes...");
  while (IsAlive()) {
    winrt::handle snapshot {CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
      dprintf("CreateToolhelp32Snapshot failed in {}", __FILE__);
      return ExitCode(1);
    }

    if (!Process32First(snapshot.get(), &process)) {
      dprintf("Process32First failed: {}", GetLastError());
      return ExitCode(2);
    }

    std::filesystem::path currentPath;

    do {
      winrt::handle processHandle {
        OpenProcess(PROCESS_ALL_ACCESS, false, process.th32ProcessID)};
      if (!processHandle) {
        continue;
      }
      DWORD bufSize = sizeof(buf);
      QueryFullProcessImageNameW(processHandle.get(), 0, buf, &bufSize);
      auto path = std::filesystem::canonical(std::wstring(buf, bufSize));

      std::scoped_lock lock(p->mGamesMutex);
      for (const auto& game: p->mGames) {
        if (path != game.path) {
          continue;
        }

        const auto friendly = game.game->GetUserFriendlyName(path);
        if (AlreadyInjected(processHandle.get(), markerDll)) {
          if (path != currentPath) {
            currentPath = path;
            wxCommandEvent ev(okEVT_GAME_CHANGED);
            ev.SetString(path.wstring());
            wxQueueEvent(p->mReceiver, ev.Clone());
          }
          break;
        }

        dprintf(
          "Found '{}' - PID {}", friendly.ToStdString(), process.th32ProcessID);

        InjectDll(processHandle.get(), markerDll);

        if (installTabletProxy) {
          InjectDll(processHandle.get(), tabletProxyDll);
        }

        if (!InjectDll(processHandle.get(), injectedDll)) {
          currentPath = path;
          dprintf("Failed to inject DLL: {}", GetLastError());
          break;
        }

        if (path == currentPath) {
          break;
        }

        currentPath = path;
        wxCommandEvent ev(okEVT_GAME_CHANGED);
        ev.SetString(path.wstring());
        wxQueueEvent(p->mReceiver, ev.Clone());
        break;
      }
    } while (Process32Next(snapshot.get(), &process));

    wxThread::This()->Sleep(200);
  }
  return ExitCode(0);
}
