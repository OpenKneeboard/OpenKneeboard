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
#include "Settings.h"

#include <ShlObj.h>

#include <filesystem>
#include <fstream>

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

static std::filesystem::path GetSettingsFilePath() {
  return GetSettingsDirectoryPath() / "Settings.json";
}

Settings Settings::Load() {
  const auto path = GetSettingsFilePath();
  if (!std::filesystem::is_regular_file(path)) {
    return {};
  }

  try {
    std::ifstream f(path.c_str());
    nlohmann::json json;
    f >> json;
    // TODO: check version
    return json;
  } catch (nlohmann::json::exception&) {
    return {};
  }
}

void Settings::Save() {
  // TODO: check not overwritting newer version
  nlohmann::json j = *this;

  const auto path = GetSettingsFilePath();
  std::ofstream f(path.c_str());
  f << std::setw(2) << j << std::endl;
}
