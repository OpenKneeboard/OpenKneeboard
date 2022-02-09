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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <OpenKneeboard/dprint.h>
#include <Windows.h>

#include <shims/winrt.h>

namespace OpenKneeboard {

static DPrintSettings gSettings;
static std::wstring gPrefixW;

void dprint(std::string_view message) {
  auto w = winrt::to_hstring(message);
  dprint(w);
}

void dprint(std::wstring_view message) {
  auto target = gSettings.target;

  if (target == DPrintSettings::Target::DEFAULT) {
#ifdef NDEBUG
    target = DPrintSettings::Target::DEBUG_STREAM;
#else
    target = DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM;
#endif;
  }

  if (
    target == DPrintSettings::Target::DEBUG_STREAM
    || target == DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM) {
    auto output = fmt::format(L"[{}] {}\n", gPrefixW, message);
    OutputDebugStringW(output.c_str());
  }

  if (
    target == DPrintSettings::Target::CONSOLE
    || target == DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM) {
    auto output = fmt::format(L"{}\n", message);
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
      output.c_str(),
      static_cast<DWORD>(output.size()),
      nullptr,
      nullptr);
  }
}

void DPrintSettings::Set(const DPrintSettings& settings) {
  gSettings = settings;
  gPrefixW = std::wstring(winrt::to_hstring(settings.prefix));
}

}// namespace OpenKneeboard
