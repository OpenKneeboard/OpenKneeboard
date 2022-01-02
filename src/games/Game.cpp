#include "OpenKneeboard/Game.h"

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

}// namespace OpenKneeboard
