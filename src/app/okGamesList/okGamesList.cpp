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
#include "okGamesList.h"

#include <OpenKneeboard/Games/DCSWorld.h>

#include "GameInjector.h"
#include "GenericGame.h"
#include "okEvents.h"
#include "okGamesList_SettingsUI.h"

using namespace OpenKneeboard;

okGamesList::okGamesList(const nlohmann::json& config) {
  mGames
    = {std::make_shared<Games::DCSWorld>(), std::make_shared<GenericGame>()};

  if (config.is_null()) {
    LoadDefaultSettings();
  } else {
    LoadSettings(config);
  }

  mInjector = std::make_unique<GameInjector>();
  AddEventListener(mInjector->evGameChanged, this->evGameChanged);
  mInjectorThread = std::jthread([this](std::stop_token stopToken) {
    mInjector->Run(stopToken);
  });
}

void okGamesList::LoadDefaultSettings() {
  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back({
        .name = game->GetUserFriendlyName(path),
        .path = path,
        .game = game,
      });
    }
  }
}

okGamesList::~okGamesList() {
}

wxWindow* okGamesList::GetSettingsUI(wxWindow* parent) {
  auto ret = new SettingsUI(parent, this);
  ret->Bind(okEVT_SETTINGS_CHANGED, [=](auto& ev) {
    mInjector->SetGameInstances(mInstances);
    wxQueueEvent(this, ev.Clone());
  });
  return ret;
}

nlohmann::json okGamesList::GetSettings() const {
  nlohmann::json games {};
  for (const auto& game: mInstances) {
    games.push_back(game.ToJson());
  }
  return {{"Configured", games}};
}

void okGamesList::LoadSettings(const nlohmann::json& config) {
  auto list = config.at("Configured");

  for (const auto& game: list) {
    mInstances.push_back(GameInstance::FromJson(game, mGames));
  }
}
