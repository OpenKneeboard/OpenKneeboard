#include "GameInstance.h"

#include "OpenKneeboard/Game.h"

namespace OpenKneeboard {

nlohmann::json GameInstance::ToJson() const {
  return {
    {"Name", this->Name},
    {"Path", this->Path.string()},
    {"Type", this->Game->GetNameForConfigFile()},
  };
}

GameInstance GameInstance::FromJson(
  const nlohmann::json& j,
  const std::vector<std::shared_ptr<OpenKneeboard::Game>>& games) {
  std::string type = j.at("Type");

  for (const auto& game: games) {
    if (type == game->GetNameForConfigFile()) {
      return {
        .Name = j.at("Name"),
        .Path = std::filesystem::path(std::string(j.at("Path"))),
        .Game = game,
      };
    }
  }
  return {};
}

}// namespace OpenKneeboard
