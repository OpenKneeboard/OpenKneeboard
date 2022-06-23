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
#include <OpenKneeboard/AppSettings.h>

namespace OpenKneeboard {

static constexpr auto WindowPositionKey {"WindowPositionV2"};
static constexpr auto LoopPagesKey {"LoopPages"};
static constexpr auto LoopTabsKey {"LoopTabs"};
static constexpr auto DualKneeboardsKey {"DualKneeboards"};
static constexpr auto AutoUpdateKey {"AutoUpdate"};

void from_json(const nlohmann::json& j, AppSettings& as) {
  if (j.is_null()) {
    return;
  }

  if (j.contains(LoopTabsKey)) {
    as.mLoopTabs = j.at(LoopTabsKey);
  }

  if (j.contains(LoopPagesKey)) {
    as.mLoopPages = j.at(LoopPagesKey);
  }
  if (j.contains(LoopTabsKey)) {
    as.mLoopTabs = j.at(LoopTabsKey);
  }
  if (j.contains(DualKneeboardsKey)) {
    as.mDualKneeboards = j.at(DualKneeboardsKey).at("Enabled");
  }

  if (!j.contains(WindowPositionKey)) {
    return;
  }

  if (j.contains(AutoUpdateKey)) {
    auto au = j.at(AutoUpdateKey);
    as.mAutoUpdate.mDisabledUntil = au.at("DisabledUntil");
    as.mAutoUpdate.mSkipVersion = au.at("SkipVersion");
    if (au.contains("ForceUpgradeTo")) {
      as.mAutoUpdate.mForceUpgradeTo = au.at("ForceUpgradeTo");
    }
  }

  auto jrect = j.at(WindowPositionKey);
  RECT rect;
  rect.left = jrect.at("Left");
  rect.top = jrect.at("Top");
  rect.right = jrect.at("Right");
  rect.bottom = jrect.at("Bottom");
  as.mWindowRect = rect;
}

void to_json(nlohmann::json& j, const AppSettings& as) {
  j = {
    {LoopPagesKey, as.mLoopPages},
    {LoopTabsKey, as.mLoopTabs},
    {
      DualKneeboardsKey,
      {
        {"Enabled", as.mDualKneeboards},
      },
    },
    {
      AutoUpdateKey,
      {
        {"DisabledUntil", as.mAutoUpdate.mDisabledUntil},
        {"SkipVersion", as.mAutoUpdate.mSkipVersion},
      },
    },
  };

  if (as.mWindowRect) {
    j[WindowPositionKey] = {
      {"Left", as.mWindowRect->left},
      {"Top", as.mWindowRect->top},
      {"Right", as.mWindowRect->right},
      {"Bottom", as.mWindowRect->bottom},
    };
  }
}

}// namespace OpenKneeboard
