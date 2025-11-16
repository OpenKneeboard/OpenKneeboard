// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <windows.h>

#include <cinttypes>
#include <optional>

namespace OpenKneeboard {

struct MainWindowInfo {
  struct VersionInfo {
    uint16_t mMajor {0};
    uint16_t mMinor {0};
    uint16_t mPatch {0};
    uint16_t mBuild {0};
    constexpr bool operator==(const VersionInfo&) const = default;
  };

  HWND mHwnd {NULL};
  VersionInfo mVersion {};

  constexpr bool operator==(const MainWindowInfo&) const = default;
};
static_assert(std::is_standard_layout_v<MainWindowInfo>);

std::optional<MainWindowInfo> GetMainWindowInfo();
std::optional<HWND> GetMainHWND();

}// namespace OpenKneeboard
