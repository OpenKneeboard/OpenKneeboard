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

void from_json(const nlohmann::json& j, AppSettings& as) {
  if (j.is_null()) {
    return;
  }

  auto jrect = j.at("WindowPosition");
  RECT rect;
  rect.left = jrect.at("Left");
  rect.top = jrect.at("Top");
  rect.right = jrect.at("Width") + rect.left;
  rect.bottom = jrect.at("Height") + rect.top;
  as.mWindowRect = rect;
}

void to_json(nlohmann::json& j, const AppSettings& as) {
  if (as.mWindowRect) {
    nlohmann::json rect;
    rect["Left"] = as.mWindowRect->left;
    rect["Top"] = as.mWindowRect->top;
    rect["Width"] = as.mWindowRect->right - as.mWindowRect->left;
    rect["Height"] = as.mWindowRect->bottom - as.mWindowRect->top;
    j["WindowPosition"] = rect;
  }
}

}// namespace OpenKneeboard
