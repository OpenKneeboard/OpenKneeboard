/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/Game.h>
#include <OpenKneeboard/GameInstance.h>
#include <OpenKneeboard/utf8.h>

namespace OpenKneeboard {

nlohmann::json GameInstance::ToJson() const {
  return {
    {"Name", this->name},
    {"Path", to_utf8(this->path)},
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
