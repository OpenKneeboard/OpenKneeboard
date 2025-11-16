// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Game.hpp>
#include <OpenKneeboard/GameInstance.hpp>

namespace OpenKneeboard {

Game::Game() {
}

Game::~Game() {
}

bool Game::MatchesPath(const std::filesystem::path& _path) const {
  auto path = std::filesystem::canonical(_path);
  for (const auto& known: this->GetInstalledPaths()) {
    if (path == known) {
      return true;
    }
  }
  return false;
}

bool Game::DiscardOculusDepthInformationDefault() const {
  return false;
}

std::shared_ptr<GameInstance> Game::CreateGameInstance(
  const std::filesystem::path& path) {
  return std::make_shared<GameInstance>(
    this->GetUserFriendlyName(path), path, this->shared_from_this());
}

std::shared_ptr<GameInstance> Game::CreateGameInstance(
  const nlohmann::json& j) {
  return std::make_shared<GameInstance>(j, this->shared_from_this());
}

}// namespace OpenKneeboard
