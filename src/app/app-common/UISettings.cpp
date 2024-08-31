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
