// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#pragma once

#include <OpenKneeboard/handles.hpp>

namespace OpenKneeboard::WindowCaptureControl {

constexpr const wchar_t WindowMessageName[] {
  L"OpenKneeboard_WindowCaptureControl"};

enum class WParam : unsigned int {
  Initialize = 1,// LPARAM: target top-level window
  StartInjection = 2,// LPARAM: target top-level window
  EndInjection = 3,
};

struct Handles {
  unique_hmodule mLibrary;
  unique_hhook mMessageHook;
  unique_hhook mWindowProcHook;

  bool IsValid() const;
};
Handles InstallHooks(HWND hwnd);

}// namespace OpenKneeboard::WindowCaptureControl
