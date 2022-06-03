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
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/GameInjector.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/GenericGame.h>
#include <windows.h>

namespace OpenKneeboard {
GamesList::GamesList(const nlohmann::json& config) {
  mGames = {std::make_shared<DCSWorld>(), std::make_shared<GenericGame>()};

  if (config.is_null()) {
    LoadDefaultSettings();
  } else {
    LoadSettings(config);
  }
}

void GamesList::StartInjector() {
  if (mInjector) {
    return;
  }

  mInjector = std::make_unique<GameInjector>();
  mInjector->SetGameInstances(mInstances);
  AddEventListener(mInjector->evGameChanged, this->evGameChanged);
  mInjectorThread = std::jthread([this](std::stop_token stopToken) {
    SetThreadDescription(GetCurrentThread(), L"GameInjector Thread");
    mInjector->Run(stopToken);
  });
}

void GamesList::LoadDefaultSettings() {
  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back(game->CreateGameInstance(path));
    }
  }
}

GamesList::~GamesList() {
}

nlohmann::json GamesList::GetSettings() const {
  nlohmann::json games {};
  for (const auto& game: mInstances) {
    games.push_back(game->ToJson());
  }
  return {{"Configured", games}};
}

void GamesList::LoadSettings(const nlohmann::json& config) {
  auto list = config.at("Configured");

  for (const auto& instance: list) {
    const std::string type = instance.at("Type");
    for (auto& game: mGames) {
      if (game->GetNameForConfigFile() == type) {
        mInstances.push_back(game->CreateGameInstance(instance));
        goto NEXT_INSTANCE;
      }
    }
  NEXT_INSTANCE:
    continue;// need statement after label
  }
}

std::vector<std::shared_ptr<Game>> GamesList::GetGames() const {
  return mGames;
}

std::vector<std::shared_ptr<GameInstance>> GamesList::GetGameInstances() const {
  return mInstances;
}

void GamesList::SetGameInstances(
  const std::vector<std::shared_ptr<GameInstance>>& instances) {
  mInstances = instances;
  if (mInjector) {
    mInjector->SetGameInstances(mInstances);
  }
  this->evSettingsChangedEvent.Emit();
}

}// namespace OpenKneeboard
