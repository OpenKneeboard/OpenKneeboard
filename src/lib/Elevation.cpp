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
#include <OpenKneeboard/Elevation.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

// clang-format off
#include <shims/winrt/base.h>
#include <shellapi.h>
// clang-format on

namespace OpenKneeboard {

static bool IsElevated(HANDLE process) noexcept {
  winrt::handle token;
  if (!OpenProcessToken(process, TOKEN_QUERY, token.put())) {
    return false;
  }
  TOKEN_ELEVATION elevation {};
  DWORD elevationSize = sizeof(TOKEN_ELEVATION);
  if (!GetTokenInformation(
        token.get(),
        TokenElevation,
        &elevation,
        sizeof(elevation),
        &elevationSize)) {
    return false;
  }
  return elevation.TokenIsElevated;
}

bool IsElevated() noexcept {
  return IsElevated(GetCurrentProcess());
}

bool IsShellElevated() noexcept {
  HWND shellWindow {GetShellWindow()};
  DWORD processID {};

  GetWindowThreadProcessId(shellWindow, &processID);
  if (!processID) {
    return true;
  }

  winrt::handle process {
    OpenProcess(PROCESS_QUERY_INFORMATION, false, processID)};
  if (!process) {
    return true;
  }
  return IsElevated(process.get());
};

DesiredElevation GetDesiredElevation() noexcept {
  constexpr DesiredElevation defaultValue {
    DesiredElevation::KeepInitialPrivileges};

  // Use the registry and HKLM so that we can reliably query while elevated
  DWORD data {};
  DWORD dataSize(sizeof(data));
  for (const auto root: {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    if (
      RegGetValueW(
        root,
        RegistrySubKey,
        L"DesiredElevation",
        RRF_RT_DWORD,
        nullptr,
        &data,
        &dataSize)
      == ERROR_SUCCESS) {
      return static_cast<DesiredElevation>(data);
    }
  }
  return defaultValue;
}

void SetDesiredElevation(DesiredElevation value) {
  if (!IsElevated()) {
    throw std::logic_error(
      "SetDesiredElevation must be called from an elevated process");
  }

  const auto data = static_cast<DWORD>(value);
  const DWORD dataSize = sizeof(data);

  RegSetKeyValueW(
    HKEY_LOCAL_MACHINE,
    RegistrySubKey,
    L"DesiredElevation",
    REG_DWORD,
    &data,
    dataSize);
}

bool RelaunchWithDesiredElevation(DesiredElevation desired, int showCommand) {
  if (desired == DesiredElevation::KeepInitialPrivileges) {
    return false;
  }

  if (IsShellElevated()) {
    return false;
  }

  wchar_t path[MAX_PATH];
  GetModuleFileNameW(NULL, path, MAX_PATH);

  if (desired == DesiredElevation::Elevated && !IsElevated()) {
    ShellExecuteW(NULL, L"runas", path, path, nullptr, showCommand);
    return true;
  }

  if (desired != DesiredElevation::NotElevated || !IsElevated()) {
    return false;
  }

  // Relaunch as child of the (not-elevated) shell
  const HWND shellWindow(GetShellWindow());
  DWORD shellProcessID {};
  GetWindowThreadProcessId(GetShellWindow(), &shellProcessID);
  if (!shellProcessID) {
    dprint("Failed to get shell process ID");
    return false;
  }

  winrt::handle shellProcess {
    OpenProcess(PROCESS_CREATE_PROCESS, false, shellProcessID)};
  if (!shellProcess) {
    return false;
  }

  SIZE_T size {};
  InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
  auto tal = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(new char[size]);
  const scope_guard freeTalMemory(
    [tal]() { delete reinterpret_cast<char*>(tal); });

  winrt::check_bool(InitializeProcThreadAttributeList(tal, 1, 0, &size));
  const scope_guard deleteTal([tal]() { DeleteProcThreadAttributeList(tal); });

  auto rawProcess = shellProcess.get();

  winrt::check_bool(UpdateProcThreadAttribute(
    tal,
    0,
    PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
    &rawProcess,
    sizeof(rawProcess),
    nullptr,
    nullptr));

  STARTUPINFOEXW startupInfo {
    .StartupInfo = {.cb = sizeof(STARTUPINFOEXW)},
    .lpAttributeList = tal,
  };
  PROCESS_INFORMATION processInfo {};
  winrt::check_bool(CreateProcessW(
    path,
    path,
    nullptr,
    nullptr,
    false,
    CREATE_NEW_CONSOLE | EXTENDED_STARTUPINFO_PRESENT,
    nullptr,
    nullptr,
    &startupInfo.StartupInfo,
    &processInfo));
  CloseHandle(processInfo.hProcess);
  CloseHandle(processInfo.hThread);
  return true;
}

}// namespace OpenKneeboard
