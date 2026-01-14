// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <nlohmann/json.hpp>

#include <cinttypes>

namespace OpenKneeboard::DCSEvents {
using GeoReal = double;

enum class Coalition {
  Neutral = 0,
  Red = 1,
  Blue = 2,
};
inline constexpr char EVT_AIRCRAFT[] = "dcs/Aircraft";
inline constexpr char EVT_INSTALL_PATH[] = "dcs/InstallPath";
inline constexpr char EVT_MISSION[] = "dcs/Mission";
inline constexpr char EVT_MISSION_TIME[] = "dcs/MissionTime";
inline constexpr char EVT_ORIGIN[] = "dcs/Origin";
inline constexpr char EVT_SELF_DATA[] = "dcs/SelfData";
inline constexpr char EVT_MESSAGE[] = "dcs/Message";
inline constexpr char EVT_SAVED_GAMES_PATH[] = "dcs/SavedGamesPath";
inline constexpr char EVT_SIMULATION_START[] = "dcs/SimulationStart";
inline constexpr char EVT_TERRAIN[] = "dcs/Terrain";

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

NLOHMANN_JSON_SERIALIZE_ENUM(
  MessageType,
  {
    {MessageType::Radio, "radio"},
    {MessageType::Show, "show"},
    {MessageType::Trigger, "trigger"},
  });
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SimulationStartEvent, missionStartTime)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  MissionTimeEvent,
  startTime,
  secondsSinceStart,
  currentTime,
  utcOffset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  MessageEvent,
  message,
  messageType,
  missionTime)
}// namespace OpenKneeboard::DCSEvents
