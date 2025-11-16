// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/AppSettings.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

OPENKNEEBOARD_DEFINE_SPARSE_JSON(BookmarkSettings, mEnabled, mLoop)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  InGameUISettings,
  mHeaderEnabled,
  mFooterEnabled,
  mFooterFrameCountEnabled,
  mBookmarksBarEnabled);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  TintSettings,
  mEnabled,
  mBrightness,
  mBrightnessStep,
  mRed,
  mGreen,
  mBlue)

// mWindowRect is handled by `*_json_postprocess` functions above
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  UISettings,
  mLoopPages,
  mLoopTabs,
  mBookmarks,
  mInGameUI,
  mTint)

}// namespace OpenKneeboard
