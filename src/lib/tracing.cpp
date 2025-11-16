// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <source_location>
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
      dprint(
        "OpenProcess(..., GetCurrentProcessID()) failed: {} @ {}",
        GetLastError(),
        std::source_location::current());
      return;
    }
    auto characterCount = static_cast<DWORD>(std::size(buffer));
    const auto result
      = QueryFullProcessImageNameW(process.get(), 0, buffer, &characterCount);
    if (result == 0) {
      dprint(
        "QueryFullProcessImageNameW() returned {}, failed with {:#018x} @ {}",
        result,
        GetLastError(),
        std::source_location::current());
    }
  });
  return sBuffer;
}

}// namespace OpenKneeboard
