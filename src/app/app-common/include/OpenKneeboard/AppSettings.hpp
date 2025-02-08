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

#include <OpenKneeboard/UISettings.hpp>

#include <OpenKneeboard/json_fwd.hpp>

#include <shims/optional>

#include <Windows.h>

#include <filesystem>

namespace OpenKneeboard {

struct AutoUpdateSettings final {
  static constexpr std::string_view StableChannel {"stable"};
  static constexpr std::string_view PreviewChannel {"preview"};

  uint64_t mDisabledUntil {0};
  std::string mSkipVersion {};
  std::string mChannel {StableChannel};

  struct Testing final {
    std::string mBaseURI {};
    std::string mFakeCurrentVersion {};
    std::string mFakeUpdateVersion {};
    bool mAlwaysCheck {false};

    constexpr bool operator==(const Testing&) const noexcept = default;
  };

  Testing mTesting {};

  constexpr bool operator==(const AutoUpdateSettings&) const noexcept = default;
};

struct AppSettings final {
  std::optional<RECT> mWindowRect;
  AutoUpdateSettings mAutoUpdate {};
  std::string mLastRunVersion;
  bool mAlwaysShowDeveloperTools {false};

  struct Deprecated {
    struct DualKneeboardSettings final {
      bool mEnabled = false;
      constexpr bool operator==(const DualKneeboardSettings&) const noexcept
        = default;
    };
    DualKneeboardSettings mDualKneeboards {};

    constexpr bool operator==(const Deprecated&) const noexcept = default;
  };
  Deprecated mDeprecated;

  bool operator==(const AppSettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(AppSettings);

}// namespace OpenKneeboard
