#include "OpenKneeboard\Games\DCSWorld.h"

#include <ShlObj.h>
#include <Windows.h>
#include <fmt/format.h>

#include "OpenKneeboard/dprint.h"

namespace OpenKneeboard::Games {

static std::filesystem::path GetDCSPath(const char* lastSubKey) {
  const auto subkey = fmt::format("SOFTWARE\\Eagle Dynamics\\{}", lastSubKey);
  char buffer[MAX_PATH];
  DWORD length = sizeof(buffer);

  if (
    RegGetValueA(
      HKEY_CURRENT_USER,
      subkey.c_str(),
      "Path",
      RRF_RT_REG_SZ,
      nullptr,
      reinterpret_cast<void*>(buffer),
      &length)
    != ERROR_SUCCESS) {
    return {};
  }

  return std::filesystem::canonical(std::filesystem::path(buffer));
}

std::filesystem::path DCSWorld::GetStablePath() {
  static std::filesystem::path sPath;
  if (sPath.empty()) {
    sPath = GetDCSPath("DCS World");
  }
  return sPath;
}

std::filesystem::path DCSWorld::GetOpenBetaPath() {
  static std::filesystem::path sPath;
  if (sPath.empty()) {
    sPath = GetDCSPath("DCS World OpenBeta");
  }
  return sPath;
}

std::filesystem::path DCSWorld::GetSavedGamesPath(
  const std::filesystem::path& installPath) {
  static std::filesystem::path sSavedGames;
  if (sSavedGames.empty()) {
    wchar_t* path = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_SavedGames, 0, NULL, &path) != S_OK) {
      return {};
    }
    if (!path) {
      return {};
    }
    sSavedGames = {std::wstring(path)};
  }

  if (installPath == GetOpenBetaPath()) {
    return sSavedGames / "DCS.openbeta";
  }

  if (installPath == GetStablePath()) {
    return sSavedGames / "DCS";
  }

  return {};
}

}// namespace OpenKneeboard::Games
