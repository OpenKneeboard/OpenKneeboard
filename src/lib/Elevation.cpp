// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Elevation.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <shims/winrt/base.h>

#include <shellapi.h>

#include <thread>

namespace OpenKneeboard {

bool IsElevated(HANDLE process) noexcept {
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
  static bool sRet;
  static std::once_flag sOnceFlag;
  std::call_once(sOnceFlag, [pValue = &sRet]() {
    *pValue = IsElevated(GetCurrentProcess());
  });
  return sRet;
}

static bool IsShellElevatedImpl() noexcept {
  DWORD processID {};

  GetWindowThreadProcessId(GetShellWindow(), &processID);
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

bool IsShellElevated() noexcept {
  static bool sRet;
  static std::once_flag sOnceFlag;
  std::call_once(
    sOnceFlag, [pRet = &sRet]() { *pRet = IsShellElevatedImpl(); });
  return sRet;
}

}// namespace OpenKneeboard
