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
#pragma once

#include <OpenKneeboard/json_fwd.h>

#include <string>
#include <unordered_map>

namespace OpenKneeboard {

struct ProfileSettings final {
  struct Profile final {
    std::string mID;
    // We store 1 profiles in folders, but a profile name might not be a valid
    // folder name, e.g 'F/A-18C', so we need to store the actual name
    std::string mName;

    constexpr auto operator<=>(const Profile&) const noexcept = default;
  };

  std::string mActiveProfile;
  std::unordered_map<std::string, Profile> mProfiles;
  bool mEnabled {false};

  std::vector<Profile> GetSortedProfiles() const;
  std::string MakeID(const std::string& name) const;

  static ProfileSettings Load();
  void Save();

  constexpr auto operator<=>(const ProfileSettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(ProfileSettings)

}// namespace OpenKneeboard
