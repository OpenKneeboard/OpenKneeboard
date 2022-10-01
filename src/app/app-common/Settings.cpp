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

static std::filesystem::path GetSettingsDirectoryPath() {
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
    = path.is_absolute() ? path : GetSettingsDirectoryPath() / path;
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
  if (!std::filesystem::exists(GetSettingsDirectoryPath() / "profiles")) {
    auto legacySettingsFile = GetSettingsDirectoryPath() / "Settings.json";
    if (std::filesystem::exists(legacySettingsFile)) {
      dprint("Migrating from legacy Settings.json");
      MaybeSetFromJSON(settings, legacySettingsFile);
      std::filesystem::rename(
        legacySettingsFile,
        GetSettingsDirectoryPath() / "LegacySettings.json.bak");
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
    = path.is_absolute() ? path : GetSettingsDirectoryPath() / path;
  if (parentValue == value) {
    if (std::filesystem::exists(fullPath)) {
      std::filesystem::remove(fullPath);
    }
    return;
  }

  const auto parentPath = fullPath.parent_path();
  if (!std::filesystem::exists(parentPath)) {
    std::filesystem::create_directories(parentPath);
  }

  nlohmann::json j = value;
  if (j.is_null()) {
    return;
  }
  std::ofstream f(fullPath.c_str());
  f << std::setw(2) << j << std::endl;
}

void Settings::Save(std::string_view profile) {
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
#define IT(cpptype, x) \
  MaybeSaveJSON(parentSettings.m##x, this->m##x, profileDir / #x##".json");
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT
}

template <>
void from_json_postprocess<Settings>(const nlohmann::json& j, Settings& s) {
  if (j.contains("DirectInputV2")) {
    s.mDirectInput = j.at("DirectInputV2");
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  Settings,
  mGames,
  mTabs,
  mApp,
  mDirectInput,
  mDoodle,
  mNonVR,
  mTabletInput,
  mVR)

}// namespace OpenKneeboard
