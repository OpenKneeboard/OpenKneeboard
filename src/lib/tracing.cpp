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
#include <source_location>
#include <shims/winrt/base.h>

#include <Windows.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <thread>

#include <processthreadsapi.h>

namespace OpenKneeboard {

wchar_t* GetFullPathForCurrentExecutable() {
  static std::once_flag sOnce;
  // `QueryFullProcessImageNameW()` requires us to provide a size; it doesn't
  // support the usual 'pass a nullptr and get the size back, then call again'
  // thing.
  static wchar_t sBuffer[MAX_PATH] {};

  std::call_once(sOnce, [&buffer = sBuffer]() {
    memset(buffer, 0, std::size(buffer) * sizeof(buffer[0]));
    // `QueryFullProcessImageNameW()` requires a real handle,
    // not the pseudo-handle returned by `GetCurrentProcess()`.
    winrt::handle process {OpenProcess(
      PROCESS_QUERY_LIMITED_INFORMATION, false, GetCurrentProcessId())};
    if (!process) {
      dprintf(
        "OpenProcess(..., GetCurrentProcessID()) failed: {} @ {}",
        GetLastError(),
        std::source_location::current());
      return;
    }
    auto characterCount = static_cast<DWORD>(std::size(buffer));
    const auto result
      = QueryFullProcessImageNameW(process.get(), 0, buffer, &characterCount);
    if (result == 0) {
      dprintf(
        "QueryFullProcessImageNameW() returned {}, failed with {:#018x} @ {}",
        result,
        GetLastError(),
        std::source_location::current());
    }
  });
  return sBuffer;
}

}// namespace OpenKneeboard
