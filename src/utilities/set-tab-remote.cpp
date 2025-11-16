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
    .prefix = "SetTab-Remote",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(pCmdLine, &argc);

  if (argc < 2 || argc > 4) {
    dprint("Usage: (id|name|position) IDENTIFIER [PAGE] [KNEEBOARD]");
    return 0;
  }

  const auto kind = std::wstring_view(argv[0]);
  const auto identifier = winrt::to_string(std::wstring_view(argv[1]));

  BaseSetTabEvent base {};

  if (argc >= 3) {
    base.mPageNumber = wcstoull(argv[2], nullptr, 10);
  }
  if (argc >= 4) {
    base.mKneeboard = wcstoull(argv[3], nullptr, 10);
  }

  if (kind == L"id") {
    APIEvent::FromStruct(SetTabByIDEvent {base, identifier}).Send();
    return 0;
  }

  if (kind == L"name") {
    APIEvent::FromStruct(SetTabByNameEvent {base, identifier}).Send();
    return 0;
  }

  if (kind == L"position") {
    const auto position = std::strtoull(identifier.c_str(), nullptr, 10);
    if (position < 1) {
      dprint("Error: position must start at 1");
      return 0;
    }
    APIEvent::FromStruct(SetTabByIndexEvent {base, position - 1}).Send();
    return 0;
  }

  dprint(L"Error: first argument must be 'id' or 'name', but '{}' given", kind);
  return 1;
}
