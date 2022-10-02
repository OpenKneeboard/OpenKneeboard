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
#include <OpenKneeboard/ProfileSettings.h>
#include <OpenKneeboard/Settings.h>
#include <OpenKneeboard/json.h>
#include <OpenKneeboard/utf8.h>

#include <fstream>

namespace OpenKneeboard {

ProfileSettings ProfileSettings::Load() {
  ProfileSettings ret;

  const auto path = Settings::GetDirectory() / "Profiles.json";
  if (std::filesystem::exists(path)) {
    std::ifstream f(path.c_str());
    nlohmann::json json;
    f >> json;
    ret = json;
  }

  if (!ret.mProfiles.contains("default")) {
    ret.mProfiles["default"] = {
      .mID = "default",
      .mName = _("Default"),
    };
  }
  if (!ret.mProfiles.contains(ret.mActiveProfile)) {
    ret.mActiveProfile = "default";
  }

  return ret;
}

void ProfileSettings::Save() {
  const auto dir = Settings::GetDirectory();
  if (!std::filesystem::exists(dir)) {
    std::filesystem::create_directories(dir);
  }
  nlohmann::json j = *this;

  std::ofstream f(dir / "Profiles.json");
  f << std::setw(2) << j << std::endl;
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ProfileSettings::Profile, mID, mName)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  ProfileSettings,
  mActiveProfile,
  mProfiles,
  mEnabled)

}// namespace OpenKneeboard
