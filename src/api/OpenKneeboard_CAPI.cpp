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

#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>

static void init() {
  static bool sInitialized = false;
  if (sInitialized) {
    return;
  }
  sInitialized = true;

  OpenKneeboard::DPrintSettings::Set({
    .prefix = "OpenKneeboard-CAPI",
  });
}

OPENKNEEBOARD_CAPI void OpenKneeboard_send_utf8(
  const char* eventName,
  size_t eventNameByteCount,
  const char* eventValue,
  size_t eventValueByteCount) {
  init();

  const OpenKneeboard::GameEvent ge {
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
  init();

  const OpenKneeboard::GameEvent ge {
    winrt::to_string(std::wstring_view {eventName, eventNameCharCount}),
    winrt::to_string(std::wstring_view {eventValue, eventValueCharCount}),
  };
  ge.Send();
}
