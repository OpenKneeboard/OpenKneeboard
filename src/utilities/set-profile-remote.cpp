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
#include <OpenKneeboard/GameEvent.h>

#include <OpenKneeboard/dprint.h>

using namespace OpenKneeboard;

#include <Windows.h>

#include <cstdlib>

#include <shellapi.h>

// We only need a standard `main()` function, but using wWinMain prevents
// a window/task bar entry from temporarily appearing
int WINAPI wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  PWSTR pCmdLine,
  int nCmdShow) {
  DPrintSettings::Set({
    .prefix = "SetProfile-Remote",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(pCmdLine, &argc);

  if (argc < 2 || argc > 4) {
    dprint("Usage: (id|name) IDENTIFIER");
    return 0;
  }

  const auto kind = std::wstring_view(argv[0]);
  const auto identifier = winrt::to_string(std::wstring_view(argv[1]));

  if (kind == L"id") {
    GameEvent::FromStruct(SetProfileByIDEvent {identifier}).Send();
    return 0;
  }

  if (kind == L"name") {
    GameEvent::FromStruct(SetProfileByNameEvent {identifier}).Send();
    return 0;
  }

  dprintf(
    L"Error: first argument must be 'id' or 'name', but '{}' given", kind);
  return 1;
}
