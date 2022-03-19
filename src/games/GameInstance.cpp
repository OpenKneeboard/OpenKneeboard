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
    {"Name", this->mName},
    {"Path", to_utf8(this->mPath)},
    {"Type", this->mGame->GetNameForConfigFile()},
  };
}

static uint64_t gNextInstanceID = 1;

GameInstance::GameInstance(
  const nlohmann::json& j,
  const std::shared_ptr<Game>& game)
  : mInstanceID(gNextInstanceID++) {
  mName = j.at("Name");
  mPath = std::filesystem::path(std::string(j.at("Path")));
  mGame = game;
}

GameInstance::GameInstance(
  const std::string& name,
  const std::filesystem::path& path,
  const std::shared_ptr<Game>& game)
  : mInstanceID(gNextInstanceID++), mName(name), mPath(path), mGame(game) {
}

}// namespace OpenKneeboard
