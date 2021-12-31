#include "OpenKneeboard\Games\DCSWorld.h"

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

wxString DCSWorld::GetUserFriendlyName(
  const std::filesystem::path& path) const {
  if (path == GetOpenBetaPath()) {
    return _("DCS World - Open Beta");
  }
  if (path == GetStablePath()) {
    return _("DCS World - Stable");
  }
  return _("DCS World");
}
std::vector<std::filesystem::path> DCSWorld::GetInstalledPaths() const {
  std::vector<std::filesystem::path> ret;
  for (const auto& path: {
         GetOpenBetaPath(),
         GetStablePath(),
       }) {
    if (!std::filesystem::is_directory(path)) {
      continue;
    }
    auto exe = path / "bin" / "DCS.exe";
    if (std::filesystem::is_regular_file(exe)) {
      ret.push_back(exe);
    }
  }
  return ret;
}

bool DCSWorld::DiscardOculusDepthInformationDefault() const {
  return true;
}

}// namespace OpenKneeboard::Games
