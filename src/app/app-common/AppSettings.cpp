/*
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
#include <OpenKneeboard/Filesystem.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  AutoUpdateSettings::Testing,
  mBaseURI,
  mFakeCurrentVersion,
  mFakeUpdateVersion,
  mAlwaysCheck)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  AutoUpdateSettings,
  mDisabledUntil,
  mSkipVersion,
  mChannel,
  mTesting)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  AppSettings::Deprecated::DualKneeboardSettings,
  mEnabled)

template <>
void from_json_postprocess<AppSettings>(
  const nlohmann::json& j,
  AppSettings& v) {
  ///// Special handling /////
  if (j.contains("WindowPositionV2")) {
    auto jrect = j.at("WindowPositionV2");
    v.mWindowRect = RECT {
      .left = jrect.at("Left"),
      .top = jrect.at("Top"),
      .right = jrect.at("Right"),
      .bottom = jrect.at("Bottom"),
    };
  }

  if (j.contains("DualKneeboards")) {
    from_json(j.at("DualKneeboards"), v.mDeprecated.mDualKneeboards);
  }

  ///// Backwards compatibility /////
  if (j.contains("AutoUpdate")) {
    auto au = j.at("AutoUpdate");
    if (au.value<bool>("HaveUsedPrereleases", false)) {
      v.mAutoUpdate.mChannel = AutoUpdateSettings::PreviewChannel;
    }
  }
}

template <>
void to_json_postprocess<AppSettings>(
  nlohmann::json& j,
  const AppSettings& parent_v,
  const AppSettings& v) {
  ///// Special handling /////
  if (v.mWindowRect) {
    j["WindowPositionV2"] = {
      {"Left", v.mWindowRect->left},
      {"Top", v.mWindowRect->top},
      {"Right", v.mWindowRect->right},
      {"Bottom", v.mWindowRect->bottom},
    };
  } else if (j.contains("WindowPositionV2")) {
    j.erase("WindowPositionV2");
  }
}

std::filesystem::path AppSettings::GetAppWebViewSourcePath() const noexcept {
  if (mAppWebViewSourcePath.empty()) {
    return GetDefaultAppWebViewSourcePath();
  }
  return mAppWebViewSourcePath;
}

std::filesystem::path AppSettings::GetDefaultAppWebViewSourcePath() noexcept {
  static std::filesystem::path sRet;
  static std::once_flag sOnce;
  std::call_once(sOnce, [&ret = *&sRet]() {
    ret = Filesystem::GetRuntimeDirectory() / "webview-content";
  });
  return sRet;
}

// mWindowRect is handled by `*_json_postprocess` functions above
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  AppSettings,
  mAutoUpdate,
  mLastRunVersion,
  mAlwaysShowDeveloperTools,
  mAppWebViewSourcePath)

}// namespace OpenKneeboard
