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
#include <OpenKneeboard/Settings.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/json.h>
#include <ShlObj.h>
#include <shims/winrt/base.h>

#include <chrono>
#include <format>
#include <fstream>
#include <shims/filesystem>

namespace OpenKneeboard {

std::filesystem::path Settings::GetDirectory() {
  static std::filesystem::path sPath;
  if (!sPath.empty()) {
    return sPath;
  }

  wchar_t* buffer = nullptr;
  if (
    !SHGetKnownFolderPath(FOLDERID_SavedGames, NULL, NULL, &buffer) == S_OK
    && buffer) {
    return {};
  }

  sPath = std::filesystem::path(std::wstring_view(buffer)) / "OpenKneeboard";
  std::filesystem::create_directories(sPath);
  return sPath;
}

template <class T>
static void MaybeSetFromJSON(T& out, const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Settings::GetDirectory() / path;
  if (!std::filesystem::exists(fullPath)) {
    return;
  }

  try {
    std::ifstream f(fullPath.c_str());
    nlohmann::json json;
    f >> json;
    out = json;
  } catch (nlohmann::json::exception& e) {
    return;
  }
}

Settings Settings::Load(std::string_view profile) {
  Settings settings;
  if (!std::filesystem::exists(Settings::GetDirectory() / "profiles")) {
    auto legacySettingsFile = Settings::GetDirectory() / "Settings.json";
    if (std::filesystem::exists(legacySettingsFile)) {
      dprint("Migrating from legacy Settings.json");
      MaybeSetFromJSON(settings, legacySettingsFile);
      std::filesystem::rename(
        legacySettingsFile,
        Settings::GetDirectory() / "LegacySettings.json.bak");
      settings.Save("default");
    }
    return std::move(settings);
  }

  if (profile != "default") {
    settings = Settings::Load("default");
  }

  const auto profileDir = std::filesystem::path {"profiles"} / profile;

#define IT(cpptype, x) \
  MaybeSetFromJSON(settings.m##x, profileDir / #x##".json");
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  return settings;
}

template <class T>
static void MaybeSaveJSON(
  const T& parentValue,
  const T& value,
  const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Settings::GetDirectory() / path;

  nlohmann::json j;
  // If a profile already modified a setting, keep that setting even if it
  // matches the parent now
  if (std::filesystem::exists(fullPath)) {
    std::ifstream f(fullPath);
    f >> j;
  }

  const auto parentPath = fullPath.parent_path();
  if (!std::filesystem::exists(parentPath)) {
    std::filesystem::create_directories(parentPath);
  }

  to_json_with_default(j, parentValue, value);
  if (j.is_object() && j.size() > 0) {
    std::ofstream f(fullPath);
    f << std::setw(2) << j << std::endl;
    return;
  }

  if (std::filesystem::exists(fullPath)) {
    std::filesystem::remove(fullPath);
  }
}

/** Used for GamesList and TabsList, where we don't want to merge configs -
 * either inherit, or overwrite */
template <>
static void MaybeSaveJSON<nlohmann::json>(
  const nlohmann::json& parentValue,
  const nlohmann::json& value,
  const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Settings::GetDirectory() / path;

  if (value == parentValue && std::filesystem::exists(fullPath)) {
    std::filesystem::remove(fullPath);
    return;
  }

  const auto parentPath = fullPath.parent_path();
  if (!std::filesystem::exists(parentPath)) {
    std::filesystem::create_directories(parentPath);
  }

  std::ofstream f(fullPath);
  f << std::setw(2) << value << std::endl;
}

void Settings::Save(std::string_view profile) const {
  const Settings previousSettings = Settings::Load(profile);

  if (previousSettings == *this) {
    return;
  }

  Settings parentSettings;
  if (profile.empty()) {
    profile = "default";
  } else if (profile != "default") {
    parentSettings = Settings::Load("default");
  }

  const auto profileDir = std::filesystem::path {"profiles"} / profile;
  if (parentSettings == *this && std::filesystem::is_directory(profileDir)) {
    // Ignore, e.g. if a file is open
    std::error_code ec;
    std::filesystem::remove_all(profileDir, ec);
    return;
  }

#define IT(cpptype, x) \
  MaybeSaveJSON(parentSettings.m##x, this->m##x, profileDir / #x##".json");
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT
}

template <>
void from_json_postprocess<Settings>(const nlohmann::json& j, Settings& s) {
  // Backwards-compatibility

  if (j.contains("DirectInputV2")) {
    s.mDirectInput = j.at("DirectInputV2");
  }
  if (j.contains("Doodle")) {
    s.mDoodles = j.at("Doodle");
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  Settings,
  mGames,
  mTabs,
  mApp,
  mDirectInput,
  mDoodles,
  mNonVR,
  mTabletInput,
  mVR)

#define IT(cpptype, name) \
  void Settings::Reset##name##Section(std::string_view profileID) { \
    if (profileID == "default") { \
      m##name = {}; \
    } else { \
      m##name = Settings::Load("default").m##name; \
    } \
    const auto path \
      = Settings::GetDirectory() / "profiles" / profileID / #name ".json"; \
    if (std::filesystem::exists(path)) { \
      std::filesystem::remove(path); \
    } \
  }
OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

}// namespace OpenKneeboard
