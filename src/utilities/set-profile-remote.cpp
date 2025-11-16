// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>

#include <OpenKneeboard/dprint.hpp>

using namespace OpenKneeboard;

#include <Windows.h>
#include <shellapi.h>

#include <cstdlib>

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
    dprint("Usage: (guid|name) IDENTIFIER");
    return 0;
  }

  const auto kind = std::wstring_view(argv[0]);
  const auto identifier = winrt::to_string(std::wstring_view(argv[1]));

  if (kind == L"id") {
    dprint("support for ID has been removed; use GUID instead");
    return 1;
  }

  if (kind == L"guid") {
    APIEvent::FromStruct(SetProfileByGUIDEvent {identifier}).Send();
    return 0;
  }

  if (kind == L"name") {
    APIEvent::FromStruct(SetProfileByNameEvent {identifier}).Send();
    return 0;
  }

  dprint(
    L"Error: first argument must be 'guid' or 'name', but '{}' given", kind);
  return 1;
}
