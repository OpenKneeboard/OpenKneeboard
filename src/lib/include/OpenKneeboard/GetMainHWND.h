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
#pragma once

#include <windows.h>

#include <cinttypes>
#include <optional>

namespace OpenKneeboard {

#pragma pack(push)
struct MainWindowInfo {
  struct VersionInfo {
    uint16_t mMajor {0};
    uint16_t mMinor {0};
    uint16_t mPatch {0};
    uint16_t mBuild {0};
    constexpr auto operator<=>(const VersionInfo&) const = default;
  };

  HWND mHwnd {NULL};
  VersionInfo mVersion {};
};
#pragma pack(pop)

std::optional<MainWindowInfo> GetMainWindowInfo();
std::optional<HWND> GetMainHWND();

}// namespace OpenKneeboard
