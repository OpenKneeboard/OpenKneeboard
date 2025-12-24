/*
 * OpenKneeboard
 *
 * Copyright (C) 2022-2023 Fred Emmott <fred@fredemmott.com>
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

#define OPENKNEEBOARD_CAPI_IMPL
#include "OpenKneeboard_CAPI.h"

#include <OpenKneeboard/APIEvent.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

static void init() {
  OpenKneeboard::DPrintSettings::Set({
    .prefix = "OpenKneeboard-CAPI",
  });

  wchar_t buf[1024];
  const auto length = GetModuleFileNameW(NULL, buf, std::size(buf));
  if (length) {
    OpenKneeboard::dprint(
      L"new API client: {}", std::wstring_view {buf, length});
  } else {
    OpenKneeboard::dprint(
      "new API client - failed to get client path: {:#018x}",
      static_cast<uint64_t>(GetLastError()));
  }
}

OPENKNEEBOARD_CAPI void OpenKneeboard_send_utf8(
  const char* eventName,
  size_t eventNameByteCount,
  const char* eventValue,
  size_t eventValueByteCount) {
  const OpenKneeboard::APIEvent ge {
    {eventName, eventNameByteCount},
    {eventValue, eventValueByteCount},
  };
  ge.Send();
}

OPENKNEEBOARD_CAPI void OpenKneeboard_send_wchar_ptr(
  const wchar_t* eventName,
  size_t eventNameCharCount,
  const wchar_t* eventValue,
  size_t eventValueCharCount) {
  const OpenKneeboard::APIEvent ge {
    winrt::to_string(std::wstring_view {eventName, eventNameCharCount}),
    winrt::to_string(std::wstring_view {eventValue, eventValueCharCount}),
  };
  ge.Send();
}

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.API.C")
 * cfaa744f-ba6f-5e56-5c91-88de46269c4b
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.API.C",
  (0xcfaa744f, 0xba6f, 0x5e56, 0x5c, 0x91, 0x88, 0xde, 0x46, 0x26, 0x9c, 0x4b));
}// namespace OpenKneeboard

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(OpenKneeboard::gTraceProvider);
      TraceLoggingWrite(
        OpenKneeboard::gTraceProvider,
        "Attached",
        TraceLoggingThisExecutable());
      init();
      break;
    case DLL_PROCESS_DETACH:
      TraceLoggingWrite(
        OpenKneeboard::gTraceProvider,
        "Detached",
        TraceLoggingThisExecutable());
      TraceLoggingUnregister(OpenKneeboard::gTraceProvider);
      break;
  }
  return TRUE;
}
