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
#include <OpenKneeboard/GameInjector.h>
#include <OpenKneeboard/Games/DCSWorld.h>
#include <OpenKneeboard/Games/GenericGame.h>
#include <OpenKneeboard/GamesList.h>

namespace OpenKneeboard {
GamesList::GamesList(const nlohmann::json& config) {
  mGames
    = {std::make_shared<Games::DCSWorld>(), std::make_shared<GenericGame>()};

  if (config.is_null()) {
    LoadDefaultSettings();
  } else {
    LoadSettings(config);
  }

  mInjector = std::make_unique<GameInjector>();
  AddEventListener(mInjector->evGameChanged, this->evGameChanged);
  mInjectorThread = std::jthread(
    [this](std::stop_token stopToken) { mInjector->Run(stopToken); });
}

void GamesList::LoadDefaultSettings() {
  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back({
        .name = game->GetUserFriendlyName(path),
        .path = path,
        .game = game,
      });
    }
  }
  mInjector->SetGameInstances(mInstances);
}

GamesList::~GamesList() {
}

nlohmann::json GamesList::GetSettings() const {
  nlohmann::json games {};
  for (const auto& game: mInstances) {
    games.push_back(game.ToJson());
  }
  return {{"Configured", games}};
}

void GamesList::LoadSettings(const nlohmann::json& config) {
  auto list = config.at("Configured");

  for (const auto& game: list) {
    mInstances.push_back(GameInstance::FromJson(game, mGames));
  }

  mInjector->SetGameInstances(mInstances);
}

std::vector<std::shared_ptr<OpenKneeboard::Game>> GamesList::GetGames() const {
  return mGames;
}

std::vector<OpenKneeboard::GameInstance> GamesList::GetGameInstances() const {
  return mInstances;
}

void GamesList::SetGameInstances(
  const std::vector<OpenKneeboard::GameInstance>& instances) {
  mInstances = instances;
  mInjector->SetGameInstances(mInstances);
}

}// namespace OpenKneeboard
