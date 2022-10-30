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
#include <OpenKneeboard/json.h>

#include <shims/filesystem>
#include <string>

namespace OpenKneeboard {

class DCSWorld final : public OpenKneeboard::Game {
 public:
  using GeoReal = double;

  const char* GetNameForConfigFile() const override;
  std::string GetUserFriendlyName(
    const std::filesystem::path&) const override final;
  virtual std::vector<std::filesystem::path> GetInstalledPaths()
    const override final;
  virtual bool DiscardOculusDepthInformationDefault() const override final;

  enum class Version { STABLE, OPEN_BETA };

  static std::filesystem::path GetInstalledPath(Version);
  static std::filesystem::path GetSavedGamesPath(Version);
  virtual bool MatchesPath(const std::filesystem::path&) const override;

  virtual std::shared_ptr<GameInstance> CreateGameInstance(
    const std::filesystem::path&) override;
  virtual std::shared_ptr<GameInstance> CreateGameInstance(
    const nlohmann::json&) override;

  enum class Coalition {
    Neutral = 0,
    Red = 1,
    Blue = 2,
  };

  static constexpr std::string_view EVT_AIRCRAFT
    = "com.fredemmott.openkneeboard.dcsext/Aircraft";
  static constexpr std::string_view EVT_INSTALL_PATH
    = "com.fredemmott.openkneeboard.dcsext/InstallPath";
  static constexpr std::string_view EVT_MISSION
    = "com.fredemmott.openkneeboard.dcsext/Mission";
  static constexpr std::string_view EVT_ORIGIN
    = "com.fredemmott.openkneeboard.dcsext/Origin";
  static constexpr std::string_view EVT_SELF_DATA
    = "com.fredemmott.openkneeboard.dcsext/SelfData";
  static constexpr std::string_view EVT_MESSAGE
    = "com.fredemmott.openkneeboard.dcsext/Message";
  static constexpr std::string_view EVT_SAVED_GAMES_PATH
    = "com.fredemmott.openkneeboard.dcsext/SavedGamesPath";
  static constexpr std::string_view EVT_SIMULATION_START
    = "com.fredemmott.openkneeboard.dcsext/SimulationStart";
  static constexpr std::string_view EVT_TERRAIN
    = "com.fredemmott.openkneeboard.dcsext/Terrain";

  struct SimulationStartEvent {
    static constexpr auto ID {EVT_SIMULATION_START};
    int64_t missionStartTime {0};
  };

  enum class MessageType {
    Invalid,
    Radio,
    Show,
    Trigger,
  };

  struct MessageEvent {
    static constexpr auto ID {EVT_MESSAGE};
    std::string message;
    MessageType messageType {MessageType::Invalid};
    int64_t missionTime {0};
  };
};

NLOHMANN_JSON_SERIALIZE_ENUM(
  DCSWorld::MessageType,
  {
    {DCSWorld::MessageType::Radio, "radio"},
    {DCSWorld::MessageType::Show, "show"},
    {DCSWorld::MessageType::Trigger, "trigger"},
  });
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  DCSWorld::SimulationStartEvent,
  missionStartTime)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  DCSWorld::MessageEvent,
  message,
  messageType,
  missionTime)

}// namespace OpenKneeboard
