// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/json_fwd.hpp>

#include <shims/winrt/base.h>

#include <format>
#include <string>
#include <unordered_map>

namespace OpenKneeboard {

struct ProfileSettings final {
  struct Profile final {
    std::string mName;
    winrt::guid mGuid = random_guid();

    static inline auto GetDirectoryName(const winrt::guid guid) {
      return std::format("{:nobraces}", guid);
    }

    inline auto GetDirectoryName() const {
      return Profile::GetDirectoryName(mGuid);
    }

    bool operator==(const Profile&) const noexcept = default;
  };

  bool mEnabled {false};
  // If you're on the first profile and press 'previous', do you go to the last
  // profile?
  bool mLoopProfiles {false};

  /// Migrated from an old version in this app launch; not saved in JSON
  bool mMigrated {false};

  std::vector<Profile> GetSortedProfiles() const;
  Profile GetActiveProfile() const;

  static ProfileSettings Load();
  void Save();

  bool operator==(const ProfileSettings&) const noexcept = default;

  winrt::guid mActiveProfile;
  winrt::guid mDefaultProfile;
  std::vector<Profile> mProfiles;
};

void from_json(const nlohmann::json& j, ProfileSettings& v);
void to_json(nlohmann::json& j, const ProfileSettings& v);

}// namespace OpenKneeboard
