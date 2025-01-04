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
