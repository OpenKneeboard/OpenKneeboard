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
#pragma once

#include <OpenKneeboard/Game.h>

#include <thread>

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/GameInstance.h>

namespace OpenKneeboard {
class GameInjector;

class GamesList final : private EventReceiver {
 private:
  std::vector<std::shared_ptr<Game>> mGames;
  std::vector<std::shared_ptr<GameInstance>> mInstances;
  std::unique_ptr<GameInjector> mInjector;
  std::jthread mInjectorThread;

  void LoadDefaultSettings();
  void LoadSettings(const nlohmann::json&);

 public:
  GamesList() = delete;
  GamesList(const nlohmann::json& config);
  virtual ~GamesList();
  virtual nlohmann::json GetSettings() const;

  std::vector<std::shared_ptr<Game>> GetGames() const;
  std::vector<std::shared_ptr<GameInstance>> GetGameInstances() const;
  void SetGameInstances(const std::vector<std::shared_ptr<GameInstance>>&);

  OpenKneeboard::Event<std::shared_ptr<GameInstance>> evGameChanged;
  OpenKneeboard::Event<> evSettingsChangedEvent;
};

}
