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
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/Settings.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/json.h>
#include <OpenKneeboard/utf8.h>

#include <shims/filesystem>
#include <shims/winrt/base.h>

#include <chrono>
#include <format>
#include <fstream>

namespace OpenKneeboard {

template <class T>
static void MaybeSetFromJSON(T& out, const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Filesystem::GetSettingsDirectory() / path;
  if (!std::filesystem::exists(fullPath)) {
    return;
  }

  try {
    std::ifstream f(fullPath.c_str());
    nlohmann::json json;
    f >> json;
    if constexpr (std::same_as<T, nlohmann::json>) {
      out = json;
    } else {
      OpenKneeboard::from_json(json, out);
    }
  } catch (const nlohmann::json::exception& e) {
    dprintf(
      "Error reading JSON from file '{}': {}", fullPath.string(), e.what());
    OPENKNEEBOARD_BREAK;
  }
}

template <class T>
static void MaybeSaveJSON(
  const T& parentValue,
  const T& value,
  const std::filesystem::path& path) {
  const auto fullPath
    = path.is_absolute() ? path : Filesystem::GetSettingsDirectory() / path;

  nlohmann::json j;
  // If a profile already modified a setting, keep that setting even if it
  // matches the parent now
  if (std::filesystem::exists(fullPath)) {
    std::ifstream f(fullPath);
    try {
      f >> j;
    } catch (const nlohmann::json::exception& e) {
      dprintf(
        "Error reading JSON from file '{}': {}", fullPath.string(), e.what());
    }
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
    = path.is_absolute() ? path : Filesystem::GetSettingsDirectory() / path;

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
  mText,
  mNonVR,
  mTabletInput,
  mViews,
  mVR)

#define IT(cpptype, name) \
  void Settings::Reset##name##Section(std::string_view profileID) { \
    if (profileID == "default") { \
      m##name = {}; \
    } else { \
      m##name = Settings::Load("default").m##name; \
    } \
    const auto path = Filesystem::GetSettingsDirectory() / "profiles" \
      / profileID / #name ".json"; \
    if (std::filesystem::exists(path)) { \
      std::filesystem::remove(path); \
    } \
  }
OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

// v1.2 -> v1.3
static void MigrateToProfiles(Settings& settings) {
  if (std::filesystem::exists(
        Filesystem::GetSettingsDirectory() / "profiles")) {
    return;
  }

  auto legacySettingsFile
    = Filesystem::GetSettingsDirectory() / "Settings.json";
  if (!std::filesystem::exists(legacySettingsFile)) {
    return;
  }

  dprint("Migrating from legacy Settings.json");
  MaybeSetFromJSON(settings, legacySettingsFile);
  std::filesystem::rename(
    legacySettingsFile,
    Filesystem::GetSettingsDirectory() / "LegacySettings.json.bak");
  settings.Save("default");
}

// v1.7 introduced 'ViewsConfig'
static bool [[nodiscard]] MigrateToViewsConfig(Settings& settings) {
  if (!settings.mViews.mViews.empty()) {
    return false;
  }

  const auto& old = settings.mVR.mDeprecated;

  IndependentViewVRConfig vrConfig {
    .mPose = old.mPrimaryLayer,
    .mMaximumPhysicalSize = {
      old.mMaxWidth,
      old.mMaxHeight,
    },
    .mEnableGazeZoom = old.mEnableGazeZoom,
    .mZoomScale = old.mZoomScale,
    .mGazeTargetScale = old.mGazeTargetScale,
    .mOpacity = old.mOpacity,
  };

  const ViewConfig primary {
    .mName = _("Kneeboard 1"),
    .mVR = ViewVRConfig::Independent(vrConfig),
    .mNonVR = ViewNonVRConfig {
      .mEnabled = true,
      .mConstraints = settings.mNonVR.mDeprecated,
    },
  };

  if (settings.mApp.mDeprecated.mDualKneeboards.mEnabled) {
    const ViewConfig secondary {
      .mName = _("Kneeboard 2"),
      .mVR = ViewVRConfig::HorizontalMirrorOf(primary.mGuid),
    };
    settings.mViews.mViews = {primary, secondary};
  } else {
    settings.mViews.mViews = {primary};
  }

  return true;
}

Settings Settings::Load(std::string_view profile) {
  Settings settings;

  MigrateToProfiles(settings);

  if (profile != "default") {
    settings = Settings::Load("default");
  }
  const auto profileDir = std::filesystem::path {"profiles"} / profile;

#define IT(cpptype, x) \
  MaybeSetFromJSON(settings.m##x, profileDir / #x##".json");
  OPENKNEEBOARD_SETTINGS_SECTIONS
#undef IT

  MigrateToViewsConfig(settings);

  return settings;
}

}// namespace OpenKneeboard
