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
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/Game.hpp>
#include <OpenKneeboard/GameInjector.hpp>
#include <OpenKneeboard/GameInstance.hpp>
#include <OpenKneeboard/GamesList.hpp>
#include <OpenKneeboard/GenericGame.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <windows.h>

#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard {
GamesList::GamesList(KneeboardState* state, const nlohmann::json& config)
  : mKneeboardState(state),
    mGames({std::make_shared<DCSWorld>(), std::make_shared<GenericGame>()}) {
  LoadSettings(config);
}

void GamesList::StartInjector() {
  if (mInjector) {
    return;
  }

  mInjector = GameInjector::Create(mKneeboardState);
  mInjector->SetGameInstances(mInstances);
  AddEventListener(
    mInjector->evGameChangedEvent,
    std::bind_front(&GamesList::OnGameChanged, this));
  mInjectorThread = {
    "GameInjector Thread",
    std::bind_front(
      [](auto injector, std::stop_token stop)
        -> winrt::Windows::Foundation::IAsyncAction {
        co_await injector->Run(stop);
      },
      mInjector),
  };
}

void GamesList::OnGameChanged(
  DWORD processID,
  const std::filesystem::path& path,
  const std::shared_ptr<GameInstance>& game) {
  if (!(processID && (!path.empty()) && game)) {
    this->evGameChangedEvent.Emit(processID, game);
    return;
  }

  if (path != game->mLastSeenPath) {
    game->mLastSeenPath = path;
    this->evSettingsChangedEvent.Emit();
  }

  this->evGameChangedEvent.Emit(processID, game);
}

void GamesList::LoadDefaultSettings() {
  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back(game->CreateGameInstance(path));
    }
  }
}

GamesList::~GamesList() {
  this->RemoveAllEventListeners();
}

nlohmann::json GamesList::GetSettings() const {
  auto games = nlohmann::json::array();

  for (const auto& game: mInstances) {
    games.push_back(game->ToJson());
  }
  return {{"Configured", games}};
}

void GamesList::LoadSettings(const nlohmann::json& config) {
  mInstances.clear();
  const scope_exit emitOnExit(
    [this]() { this->evSettingsChangedEvent.Emit(); });

  if (config.is_null()) {
    LoadDefaultSettings();
    return;
  }

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
