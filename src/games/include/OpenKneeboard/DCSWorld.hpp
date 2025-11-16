// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Game.hpp>

#include <OpenKneeboard/json.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

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

  // For example, the Lua aircraft 'FA-18C_hornet' is in the
  // 'FA-18C' module, and wants that in the kneeboard paths
  static std::string GetModuleNameForLuaAircraft(const std::string&);

  virtual std::shared_ptr<GameInstance> CreateGameInstance(
    const std::filesystem::path&) override;
  virtual std::shared_ptr<GameInstance> CreateGameInstance(
    const nlohmann::json&) override;

  enum class Coalition {
    Neutral = 0,
    Red = 1,
    Blue = 2,
  };

  static constexpr char EVT_AIRCRAFT[] = "dcs/Aircraft";
  static constexpr char EVT_INSTALL_PATH[] = "dcs/InstallPath";
  static constexpr char EVT_MISSION[] = "dcs/Mission";
  static constexpr char EVT_MISSION_TIME[] = "dcs/MissionTime";
  static constexpr char EVT_ORIGIN[] = "dcs/Origin";
  static constexpr char EVT_SELF_DATA[] = "dcs/SelfData";
  static constexpr char EVT_MESSAGE[] = "dcs/Message";
  static constexpr char EVT_SAVED_GAMES_PATH[] = "dcs/SavedGamesPath";
  static constexpr char EVT_SIMULATION_START[] = "dcs/SimulationStart";
  static constexpr char EVT_TERRAIN[] = "dcs/Terrain";

  struct SimulationStartEvent {
    static constexpr auto ID {EVT_SIMULATION_START};
    int64_t missionStartTime {0};
  };

  struct MissionTimeEvent {
    static constexpr auto ID {EVT_MISSION_TIME};
    int64_t startTime {};
    float secondsSinceStart {};
    float currentTime {};
    int64_t utcOffset {};
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
  DCSWorld::MissionTimeEvent,
  startTime,
  secondsSinceStart,
  currentTime,
  utcOffset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  DCSWorld::MessageEvent,
  message,
  messageType,
  missionTime)

}// namespace OpenKneeboard
