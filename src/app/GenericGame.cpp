#include "GenericGame.h"

namespace OpenKneeboard {

std::vector<std::filesystem::path> GenericGame::GetInstalledPaths() const {
  return {};
}

wxString GenericGame::GetUserFriendlyName(
  const std::filesystem::path& path) const {
  return path.stem().wstring();
}

const char* GenericGame::GetNameForConfigFile() const {
  return "Generic";
}

}// namespace OpenKneeboard
