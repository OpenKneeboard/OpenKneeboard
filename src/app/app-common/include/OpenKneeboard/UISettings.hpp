// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/json_fwd.hpp>

#include <Windows.h>

namespace OpenKneeboard {

struct BookmarkSettings final {
  bool mEnabled = true;
  bool mLoop = false;

  constexpr bool operator==(const BookmarkSettings&) const noexcept = default;
};

struct InGameUISettings final {
  bool mHeaderEnabled = true;
  bool mFooterEnabled = true;
  bool mFooterFrameCountEnabled = false;
  bool mBookmarksBarEnabled = true;

  constexpr bool operator==(const InGameUISettings&) const noexcept = default;
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

  constexpr bool operator==(const TintSettings&) const noexcept = default;
};

struct UISettings {
  bool mLoopPages {false};
  bool mLoopTabs {false};

  BookmarkSettings mBookmarks {};
  InGameUISettings mInGameUI {};
  TintSettings mTint {};

  constexpr bool operator==(const UISettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(UISettings);

}// namespace OpenKneeboard
