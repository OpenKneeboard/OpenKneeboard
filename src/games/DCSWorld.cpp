// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/DCSWorldInstance.hpp>
#include <OpenKneeboard/Filesystem.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>
#include <ShlObj.h>

#include <format>
#include <fstream>
#include <unordered_map>

namespace OpenKneeboard {

std::string DCSWorld::GetModuleNameForLuaAircraft(const std::string& luaName) {
  using mapping_t = std::unordered_map<std::string, std::string>;
  static std::optional<mapping_t> sMapping;
  if (sMapping) {
    if (sMapping->contains(luaName)) {
      return sMapping->at(luaName);
    }
    return luaName;
  }

  const std::filesystem::path fileName = "DCS-Aircraft-Mapping.json";
  const std::vector<std::filesystem::path> dirs = {
    // User-override
    Filesystem::GetSettingsDirectory(),
    // As-installed
    Filesystem::GetImmutableDataDirectory(),
  };

  dprint("Loading DCS aircraft mapping...");
  for (const auto& dir: dirs) {
    const auto path = dir / fileName;
    dprint("Trying {}...", path);
    if (!std::filesystem::exists(path)) {
      dprint("... not found.");
      continue;
    }

    dprint("... reading JSON.");

    try {
      std::ifstream f(path.c_str());
      nlohmann::json json;
      f >> json;
      mapping_t data = json;
      sMapping = std::move(data);
      dprint("... done.");
      break;
    } catch (const nlohmann::json::exception& e) {
      dprint("... JSON exception: {}", e.what());
    } catch (const std::exception& e) {
      dprint("... standard exception: {}", e.what());
    }
  }

  if (!sMapping) {
    dprint("Failed to load any mapping file.");
    sMapping = mapping_t {};
  }

  if (sMapping && sMapping->contains(luaName)) {
    return sMapping->at(luaName);
  }

  dprint("Aircraft mapping:");
  for (const auto& [key, value]: *sMapping) {
    dprint("- {} -> {}", key, value);
  }

  return luaName;
}// namespace OpenKneeboard

static std::filesystem::path GetDCSPath(const char* lastSubKey) {
  const auto subkey = std::format("SOFTWARE\\Eagle Dynamics\\{}", lastSubKey);
  const auto wSubKey = winrt::to_hstring(subkey);

  wchar_t buffer[MAX_PATH];
  DWORD length = sizeof(buffer) * sizeof(buffer[0]);

  if (
    RegGetValueW(
      HKEY_CURRENT_USER,
      wSubKey.c_str(),
      L"Path",
      RRF_RT_REG_SZ,
      nullptr,
      reinterpret_cast<void*>(buffer),
      &length)
    != ERROR_SUCCESS) {
    return {};
  }

  const auto path = std::filesystem::path(
    std::wstring_view(buffer, length / sizeof(buffer[0])));
  if (!std::filesystem::is_directory(path)) {
    return {};
  }

  return std::filesystem::canonical(path);
}

bool DCSWorld::MatchesPath(const std::filesystem::path& path) const {
  return path.filename() == "DCS.exe";
}

std::filesystem::path DCSWorld::GetInstalledPath(Version version) {
  switch (version) {
    case Version::OPEN_BETA:
      return GetDCSPath("DCS World OpenBeta");
    case Version::STABLE:
      return GetDCSPath("DCS World");
  }
  return {};
}

std::filesystem::path DCSWorld::GetSavedGamesPath(Version version) {
  const auto savedGames = Filesystem::GetKnownFolderPath<FOLDERID_SavedGames>();

  switch (version) {
    case Version::OPEN_BETA:
      return savedGames / "DCS.openbeta";
    case Version::STABLE:
      return savedGames / "DCS";
  }
  return {};
}

std::string DCSWorld::GetUserFriendlyName(
  const std::filesystem::path& _path) const {
  const auto path = std::filesystem::canonical(_path);
  if (path == GetInstalledPath(Version::OPEN_BETA) / "bin" / "DCS.exe") {
    return _("DCS World - Open Beta");
  }
  if (path == GetInstalledPath(Version::OPEN_BETA) / "bin-mt" / "DCS.exe") {
    return _("DCS World - Open Beta - Multi-Threaded");
  }
  if (path == GetInstalledPath(Version::STABLE) / "bin" / "DCS.exe") {
    return _("DCS World - Stable");
  }
  if (path == GetInstalledPath(Version::STABLE) / "bin-mt" / "DCS.exe") {
    return _("DCS World - Stable - Multi-Threaded");
  }
  return _("DCS World");
}

std::vector<std::filesystem::path> DCSWorld::GetInstalledPaths() const {
  std::vector<std::filesystem::path> ret;
  for (const auto version: {Version::OPEN_BETA, Version::STABLE}) {
    const auto root = GetInstalledPath(version);
    if (!std::filesystem::is_directory(root)) {
      continue;
    }
    auto exe = root / "bin" / "DCS.exe";
    if (std::filesystem::is_regular_file(exe)) {
      ret.push_back(exe);
    }
    auto mtexe = root / "bin-mt" / "DCS.exe";
    if (std::filesystem::is_regular_file(mtexe)) {
      ret.push_back(mtexe);
    }
  }
  return ret;
}

bool DCSWorld::DiscardOculusDepthInformationDefault() const {
  return true;
}

const char* DCSWorld::GetNameForConfigFile() const {
  return "DCSWorld";
}

std::shared_ptr<GameInstance> DCSWorld::CreateGameInstance(
  const std::filesystem::path& path) {
  return std::make_shared<DCSWorldInstance>(
    this->GetUserFriendlyName(path), path, this->shared_from_this());
}

std::shared_ptr<GameInstance> DCSWorld::CreateGameInstance(
  const nlohmann::json& j) {
  return std::make_shared<DCSWorldInstance>(j, this->shared_from_this());
}

}// namespace OpenKneeboard
