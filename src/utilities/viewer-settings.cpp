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
#include "viewer-settings.h"

#include <OpenKneeboard/Filesystem.h>

#include <OpenKneeboard/json.h>
#include <OpenKneeboard/utf8.h>

#include <algorithm>
#include <format>
#include <fstream>

namespace OpenKneeboard {
ViewerSettings ViewerSettings::Load() {
  ViewerSettings ret;

  const auto path = Filesystem::GetSettingsDirectory() / "Viewer.json";
  if (std::filesystem::exists(path)) {
    std::ifstream f(path.c_str());
    nlohmann::json json;
    f >> json;
    // ret = json; // this doesnt actually update ret

    ret.mWindowWidth = json["mWindowWidth"];
    ret.mWindowHeight = json["mWindowHeight"];
    ret.mWindowX = json["mWindowX"];
    ret.mWindowY = json["mWindowY"];
    ret.mStreamerMode = json["mStreamerMode"];
    ret.mFillMode = json["mFillMode"];
  }

  return ret;
}

void ViewerSettings::Save() {
  const auto dir = Filesystem::GetSettingsDirectory();
  if (!std::filesystem::exists(dir)) {
    std::filesystem::create_directories(dir);
  }
  nlohmann::json j;// = *this; // this results in null

  j["mWindowWidth"] = mWindowWidth;
  j["mWindowHeight"] = mWindowHeight;
  j["mWindowX"] = mWindowX;
  j["mWindowY"] = mWindowY;
  j["mStreamerMode"] = mStreamerMode;
  j["mFillMode"] = mFillMode;

  std::ofstream f(dir / "Viewer.json");
  f << std::setw(2) << j << std::endl;
}

// I'm guessing this is supposed to make the conversion from json to object work
// but for some reason it's not working, so meh
// OPENKNEEBOARD_DEFINE_SPARSE_JSON(
//   ViewerSettings,
//   mWindowWidth,
//   mWindowHeight,
//   mWindowX,
//   mWindowY,
//   mStreamerMode,
//   mFillMode)

}// namespace OpenKneeboard
