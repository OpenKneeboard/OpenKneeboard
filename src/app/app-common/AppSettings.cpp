// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
  const AppSettings& /*parent_v*/,
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

// mWindowRect is handled by `*_json_postprocess` functions above
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  AppSettings,
  mAutoUpdate,
  mLastRunVersion,
  mAlwaysShowDeveloperTools)

}// namespace OpenKneeboard
