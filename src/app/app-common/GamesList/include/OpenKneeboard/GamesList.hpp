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

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/RunnerThread.hpp>

#include <shims/nlohmann/json_fwd.hpp>

#include <expected>
#include <thread>

namespace OpenKneeboard {
class Game;
struct GameInstance;
class GameInjector;
class KneeboardState;

class GamesList final : private EventReceiver {
 private:
  KneeboardState* mKneeboardState {nullptr};
  std::vector<std::shared_ptr<Game>> mGames;
  std::vector<std::shared_ptr<GameInstance>> mInstances;
  std::shared_ptr<GameInjector> mInjector;
  RunnerThread mInjectorThread;

  void LoadDefaultSettings();

 public:
  GamesList() = delete;
  GamesList(KneeboardState*, const nlohmann::json& config);
  virtual ~GamesList();
  virtual nlohmann::json GetSettings() const;
  void LoadSettings(const nlohmann::json&);

  void StartInjector();

  std::vector<std::shared_ptr<Game>> GetGames() const;
  std::vector<std::shared_ptr<GameInstance>> GetGameInstances() const;
  void SetGameInstances(const std::vector<std::shared_ptr<GameInstance>>&);

  Event<DWORD, const std::shared_ptr<GameInstance>&> evGameChangedEvent;
  Event<> evSettingsChangedEvent;

  enum class PathPatternError {
    NotAGame,
    Launcher,
  };
  /// Correct common misconfigurations.
  static std::expected<std::string, PathPatternError> FixPathPattern(
    std::string_view pattern);

 private:
  void OnGameChanged(
    DWORD,
    const std::filesystem::path&,
    const std::shared_ptr<GameInstance>&);
};

}// namespace OpenKneeboard
