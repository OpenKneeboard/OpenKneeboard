#pragma once

#include <json.hpp>
#include <memory>

#include "OpenKneeboard/Game.h"

namespace OpenKneeboard {
struct GameInstance {
  std::string Name;
  std::filesystem::path Path;
  std::shared_ptr<Game> Game;

  nlohmann::json ToJson() const;
  static GameInstance FromJson(
    const nlohmann::json&,
    const std::vector<std::shared_ptr<OpenKneeboard::Game>>&);
};
};// namespace OpenKneeboard
