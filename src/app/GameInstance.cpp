#include "GameInstance.h"

#include "OpenKneeboard/Game.h"

namespace OpenKneeboard {

nlohmann::json GameInstance::ToJson() const {
  return {
    {"Name", this->name},
    {"Path", this->path.string()},
    {"Type", this->game->GetNameForConfigFile()},
  };
}

GameInstance GameInstance::FromJson(
  const nlohmann::json& j,
  const std::vector<std::shared_ptr<OpenKneeboard::Game>>& games) {
  std::string type = j.at("Type");

  for (const auto& game: games) {
    if (type == game->GetNameForConfigFile()) {
      return {
        .name = j.at("Name"),
        .path = std::filesystem::path(std::string(j.at("Path"))),
        .game = game,
      };
    }
  }
  return {};
}

}// namespace OpenKneeboard
