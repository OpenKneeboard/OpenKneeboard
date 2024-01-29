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

#include <OpenKneeboard/json_fwd.h>

#include <shims/Windows.h>
#include <shims/optional>

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

    constexpr auto operator<=>(const Testing&) const noexcept = default;
  };

  Testing mTesting {};

  constexpr auto operator<=>(const AutoUpdateSettings&) const noexcept
    = default;
};

struct AppSettings final {
  struct DualKneeboardSettings final {
    bool mEnabled = false;
    constexpr auto operator<=>(const DualKneeboardSettings&) const noexcept
      = default;
  };

  struct BookmarkSettings final {
    bool mEnabled = true;
    bool mLoop = false;

    constexpr auto operator<=>(const BookmarkSettings&) const noexcept
      = default;
  };

  struct InGameUISettings final {
    bool mHeaderEnabled = true;
    bool mFooterEnabled = true;
    bool mFooterFrameCountEnabled = false;
    bool mBookmarksBarEnabled = true;

    constexpr auto operator<=>(const InGameUISettings&) const noexcept
      = default;
  };

  struct TintSettings final {
    bool mEnabled = false;
    float mBrightness = 1.0f;
    float mBrightnessStep = 0.1f;
    float mRed = 1.0f;
    float mGreen = 1.0f;
    float mBlue = 1.0f;

    // No alpha given there are separate VR- and Non-VR-
    // settings for alpha.

    constexpr auto operator<=>(const TintSettings&) const noexcept = default;
  };

  std::optional<RECT> mWindowRect;
  bool mLoopPages {false};
  bool mLoopTabs {false};
  AutoUpdateSettings mAutoUpdate {};
  BookmarkSettings mBookmarks {};
  InGameUISettings mInGameUI {};
  TintSettings mTint {};
  std::string mLastRunVersion;

  struct Deprecated {
    DualKneeboardSettings mDualKneeboards {};
    constexpr auto operator<=>(const Deprecated&) const noexcept = default;
  };
  Deprecated mDeprecated;

  constexpr auto operator<=>(const AppSettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(AppSettings);

}// namespace OpenKneeboard
