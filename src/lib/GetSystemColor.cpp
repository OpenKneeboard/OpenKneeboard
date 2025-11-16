// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/GetSystemColor.hpp>

#include <Windows.h>

namespace OpenKneeboard {

D2D1_COLOR_F GetSystemColor(int index) {
  auto color = ::GetSysColor(index);
  return {
    GetRValue(color) / 255.0f,
    GetGValue(color) / 255.0f,
    GetBValue(color) / 255.0f,
    1.0f,
  };
}

}// namespace OpenKneeboard
