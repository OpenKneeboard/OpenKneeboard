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

void from_json(const nlohmann::json& j, AppSettings& as) {
  if (j.is_null()) {
    return;
  }

  if (j.contains(LoopPagesKey)) {
    as.mLoopPages = j.at(LoopPagesKey);
  }
  if (j.contains(LoopTabsKey)) {
    as.mLoopTabs = j.at(LoopTabsKey);
  }

  if (!j.contains(WindowPositionKey)) {
    return;
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
  j[LoopPagesKey] = as.mLoopPages;
  j[LoopTabsKey] = as.mLoopTabs;

  if (as.mWindowRect) {
    nlohmann::json rect;
    rect["Left"] = as.mWindowRect->left;
    rect["Top"] = as.mWindowRect->top;
    rect["Right"] = as.mWindowRect->right;
    rect["Bottom"] = as.mWindowRect->bottom;
    j[WindowPositionKey] = rect;
  }
}

}// namespace OpenKneeboard
