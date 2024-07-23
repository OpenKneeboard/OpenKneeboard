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
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/ProfileSettings.hpp>
#include <OpenKneeboard/Settings.hpp>

#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <algorithm>
#include <format>
#include <fstream>

namespace OpenKneeboard {

ProfileSettings::Profile ProfileSettings::GetActiveProfile() const {
  return mProfiles.at(mActiveProfile);
}

std::vector<ProfileSettings::Profile> ProfileSettings::GetSortedProfiles()
  const {
  std::vector<ProfileSettings::Profile> ret;
  for (const auto& [id, profile]: mProfiles) {
    ret.push_back(profile);
  }
  std::ranges::sort(ret, [](const auto& a, const auto& b) {
    if (a.mID == "default") {
      return true;
    }
    if (b.mID == "default") {
      return false;
    }
    return a.mName < b.mName;
  });
  return ret;
}

std::string ProfileSettings::MakeID(const std::string& name) const {
  std::string id;
  for (char it: name) {
    it = std::tolower(it);
    if ((it >= 'a' && it <= 'z') || (it >= '0' && it <= '9')) {
      id += it;
    }
  }

  if (id.empty()) {
    id = "empty";
  }
  if (!mProfiles.contains(id)) {
    return id;
  }

  size_t i = 1;
  while (true) {
    const auto nextID = std::format("{}_{}", id, i++);
    if (!mProfiles.contains(nextID)) {
      return nextID;
    }
  }
}

ProfileSettings ProfileSettings::Load() {
  bool migrated = false;
  ProfileSettings ret;

  const auto path = Filesystem::GetSettingsDirectory() / "Profiles.json";
  if (std::filesystem::exists(path)) {
    std::ifstream f(path.c_str());
    nlohmann::json json;
    f >> json;
    ret = json;

    for (const auto& profile: json.at("Profiles")) {
      if (!profile.contains("Guid")) {
        migrated = true;
        break;
      }
    }
  }

  if (!ret.mProfiles.contains("default")) {
    ret.mProfiles["default"] = {
      .mID = "default",
      .mName = _("Default"),
    };
    migrated = true;
  }
  if (!ret.mProfiles.contains(ret.mActiveProfile)) {
    ret.mActiveProfile = "default";
  }

  if (migrated) {
    ret.Save();
  }

  return ret;
}

void ProfileSettings::Save() {
  const auto dir = Filesystem::GetSettingsDirectory();
  if (!std::filesystem::exists(dir)) {
    std::filesystem::create_directories(dir);
  }
  nlohmann::json j = *this;

  std::ofstream f(dir / "Profiles.json");
  f << std::setw(2) << j << std::endl;
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ProfileSettings::Profile, mID, mName, mGuid)
OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  ProfileSettings,
  mActiveProfile,
  mProfiles,
  mLoopProfiles,
  mEnabled)

}// namespace OpenKneeboard
