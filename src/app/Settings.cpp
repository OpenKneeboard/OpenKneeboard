#include "Settings.h"

#include <ShlObj.h>

#include <filesystem>
#include <fstream>

static std::filesystem::path GetSettingsDirectoryPath() {
  static std::filesystem::path sPath;
  if (!sPath.empty()) {
    return sPath;
  }

  wchar_t* buffer;
  if (
    !SHGetKnownFolderPath(FOLDERID_SavedGames, NULL, NULL, &buffer) == S_OK
    && buffer) {
    return {};
  }

  sPath = std::filesystem::path(std::wstring(buffer)) / "OpenKneeboard";
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
