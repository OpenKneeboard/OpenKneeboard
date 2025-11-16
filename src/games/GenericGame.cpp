// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/GenericGame.hpp>

#include <OpenKneeboard/utf8.hpp>

namespace OpenKneeboard {

bool GenericGame::MatchesPath(const std::filesystem::path&) const {
  return true;
}

std::vector<std::filesystem::path> GenericGame::GetInstalledPaths() const {
  return {};
}

std::string GenericGame::GetUserFriendlyName(
  const std::filesystem::path& path) const {
  return to_utf8(path.stem());
}

const char* GenericGame::GetNameForConfigFile() const {
  return "Generic";
}

}// namespace OpenKneeboard
