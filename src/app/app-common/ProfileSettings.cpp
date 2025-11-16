// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/ProfileSettings.hpp>
#include <OpenKneeboard/Settings.hpp>

#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/json.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <algorithm>
#include <format>
#include <fstream>

namespace OpenKneeboard {

ProfileSettings::Profile ProfileSettings::GetActiveProfile() const {
  return *std::ranges::find(mProfiles, mActiveProfile, &Profile::mGuid);
}

std::vector<ProfileSettings::Profile> ProfileSettings::GetSortedProfiles()
  const {
  std::vector<ProfileSettings::Profile> ret = mProfiles;
  std::ranges::sort(ret, [this](const auto& a, const auto& b) {
    if (a.mGuid == mDefaultProfile) {
      return true;
    }
    if (b.mGuid == mDefaultProfile) {
      return false;
    }
    return a.mName < b.mName;
  });
  return ret;
}

ProfileSettings ProfileSettings::Load() {
  ProfileSettings ret;

  const auto path = Filesystem::GetSettingsDirectory() / "Profiles.json";
  if (std::filesystem::exists(path)) {
    std::ifstream f(path.c_str());
    nlohmann::json json;
    f >> json;
    ret = json;

    for (const auto& profile: json.at("Profiles")) {
      if (!profile.contains("Guid")) {
        ret.mMigrated = true;
        break;
      }
    }
  }

  if (
    ret.mDefaultProfile == winrt::guid {}
    || !std::ranges::contains(
      ret.mProfiles, ret.mDefaultProfile, &Profile::mGuid)) {
    Profile profile {_("Default")};
    ret.mActiveProfile = profile.mGuid;
    ret.mDefaultProfile = profile.mGuid;
    ret.mProfiles.push_back(std::move(profile));
    ret.mMigrated = true;
  }

  if (!std::ranges::contains(
        ret.mProfiles, ret.mActiveProfile, &Profile::mGuid)) {
    ret.mActiveProfile = ret.mDefaultProfile;
  }

  if (ret.mMigrated) {
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

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ProfileSettings::Profile, mName, mGuid)

void from_json(const nlohmann::json& j, ProfileSettings& v) {
  if (j.contains("LoopProfiles")) {
    v.mLoopProfiles = j.at("LoopProfiles");
  }
  if (j.contains("Enabled")) {
    v.mEnabled = j.at("Enabled");
  }

  std::vector<std::filesystem::path> toRemove;
  if (j.contains("Profiles")) {
    auto jj = j.at("Profiles");
    if (jj.type() == nlohmann::json::value_t::array) {
      // v1.9+
      v.mProfiles = jj;
    } else {
      // v1.8: map of folder name -> Profile
      std::unordered_map<std::string, ProfileSettings::Profile> oldValue;
      oldValue = jj;

      std::unordered_set<winrt::guid> guids;

      const auto baseDir = Filesystem::GetSettingsDirectory() / "Profiles";
      for (auto&& [subfolder, profile]: oldValue) {
        // Part of the reason for changing the structure of profiles is because
        // of people manually editing the profiles.json file incorrectly; the
        // new structure makes some common mistakes impossible.
        //
        // While editing OpenKneeboard configuration files outside of
        // OpenKneeboard is not supported, duplicate GUIDs are a particularly
        // common case.
        if (guids.contains(profile.mGuid)) {
          const auto newGUID = random_guid();
          dprint.Warning(
            "Profile '{}' has duplicate GUID {} - changing to new GUID {}",
            profile.mName,
            profile.mGuid,
            newGUID);
          profile.mGuid = newGUID;
        }
        guids.emplace(profile.mGuid);

        const auto oldPath = baseDir / subfolder;
        const auto newPath = baseDir / profile.GetDirectoryName();
        // Profiles with no changes do not necessarily have a settings folder
        if (!std::filesystem::exists(oldPath)) {
          dprint.Warning(
            "Migrated empty profile {} ('{}')", profile.mGuid, profile.mName);
        } else {
          try {
            std::filesystem::copy(
              oldPath, newPath, std::filesystem::copy_options::recursive);
          } catch (const std::filesystem::filesystem_error& e) {
            dprint.Warning(
              "Error migrating profile from `{}` to `{}`: {} ({})",
              oldPath,
              newPath,
              e.what(),
              e.code().value());
          }
          toRemove.push_back(oldPath);
        }
        v.mProfiles.push_back(profile);
      }
      v.mDefaultProfile = oldValue.at("default").mGuid;
      v.mActiveProfile
        = oldValue.at(j.at("ActiveProfile").get<std::string>()).mGuid;
      v.mMigrated = true;
    }
  }

  if (j.contains("ActiveProfile")) {
    try {
      v.mActiveProfile = j.at("ActiveProfile");
    } catch (...) {
      // Versions < 1.9 used folder name rather than GUID; this is handled above
    }
  }

  if (j.contains("DefaultProfile")) {
    v.mDefaultProfile = j.at("DefaultProfile");
  }

  for (auto&& it: toRemove) {
    std::filesystem::remove_all(it);
  }
}

void to_json(nlohmann::json& j, const ProfileSettings& v) {
  j.update({
    {"LoopProfiles", v.mLoopProfiles},
    {"Enabled", v.mEnabled},
    {"Profiles", v.mProfiles},
    {"ActiveProfile", v.mActiveProfile},
    {"DefaultProfile", v.mDefaultProfile},
  });
}

}// namespace OpenKneeboard
