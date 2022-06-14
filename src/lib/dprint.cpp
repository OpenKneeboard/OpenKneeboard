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
#include <OpenKneeboard/dprint.h>
#include <Windows.h>
#include <shims/winrt.h>

#include <filesystem>
#include <optional>

namespace OpenKneeboard {

static DPrintSettings gSettings;
static std::wstring gPrefixW;

static bool IsDebugStreamEnabledInRegistry() {
  static std::optional<bool> sCache;
  if (sCache) {
    return *sCache;
  }
  DWORD value = 0;
  for (auto hkey: {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    DWORD size = sizeof(value);
    if (
      RegGetValueW(
        hkey,
        L"SOFTWARE\\Fred Emmott\\OpenKneeboard",
        L"AlwaysWriteToDebugStream",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size)
      == ERROR_SUCCESS) {
      break;
    }
  }
  *sCache = (value != 0);
  return *sCache;
}

static inline bool IsDebugStreamEnabled() {
#ifdef DEBUG
  return true;
#endif
  return IsDebugStreamEnabledInRegistry() || IsDebuggerPresent();
}

static inline bool IsConsoleOutputEnabled() {
#ifdef DEBUG
  return true;
#endif
  return gSettings.consoleOutput == DPrintSettings::ConsoleOutputMode::ALWAYS;
}

void dprint(std::string_view message) {
  auto w = winrt::to_hstring(message);
  dprint(w);
}

void dprint(std::wstring_view message) {
  if (IsDebugStreamEnabled()) {
    auto output = fmt::format(L"[{}] {}\n", gPrefixW, message);
    OutputDebugStringW(output.c_str());
  }

  if (IsConsoleOutputEnabled()) {
    if (AllocConsole()) {
      // Gets detour trace working too :)
      freopen("CONIN$", "r", stdin);
      freopen("CONOUT$", "w", stdout);
      freopen("CONOUT$", "w", stderr);
    }
    auto handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
      return;
    }
    WriteConsoleW(
      handle,
      message.data(),
      static_cast<DWORD>(message.size()),
      nullptr,
      nullptr);
  }
}

void DPrintSettings::Set(const DPrintSettings& settings) {
  gSettings = settings;
  gPrefixW = std::wstring(winrt::to_hstring(settings.prefix));
}

}// namespace OpenKneeboard
