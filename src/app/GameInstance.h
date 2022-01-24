#pragma once

#include <OpenKneeboard/Game.h>

#include <nlohmann/json.hpp>
#include <memory>

namespace OpenKneeboard {
struct GameInstance {
  std::string name;
  std::filesystem::path path;
  std::shared_ptr<Game> game;

  nlohmann::json ToJson() const;
  static GameInstance FromJson(
    const nlohmann::json&,
    const std::vector<std::shared_ptr<OpenKneeboard::Game>>&);
};
};// namespace OpenKneeboard
