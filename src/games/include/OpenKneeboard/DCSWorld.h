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

#include <shims/filesystem>
#include <string>

namespace OpenKneeboard {

class DCSWorld final : public OpenKneeboard::Game {
 public:
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

  static constexpr char EVT_AIRCRAFT[]
    = "com.fredemmott.openkneeboard.dcsext/Aircraft";
  static constexpr char EVT_INSTALL_PATH[]
    = "com.fredemmott.openkneeboard.dcsext/InstallPath";
  static constexpr char EVT_MISSION[]
    = "com.fredemmott.openkneeboard.dcsext/Mission";
  static constexpr char EVT_ORIGIN[]
    = "com.fredemmott.openkneeboard.dcsext/Origin";
  static constexpr char EVT_SELF_DATA[]
    = "com.fredemmott.openkneeboard.dcsext/SelfData";
  static constexpr char EVT_RADIO_MESSAGE[]
    = "com.fredemmott.openkneeboard.dcsext/RadioMessage";
  static constexpr char EVT_SAVED_GAMES_PATH[]
    = "com.fredemmott.openkneeboard.dcsext/SavedGamesPath";
  static constexpr char EVT_SIMULATION_START[]
    = "com.fredemmott.openkneeboard.dcsext/SimulationStart";
  static constexpr char EVT_TERRAIN[]
    = "com.fredemmott.openkneeboard.dcsext/Terrain";

  enum class Coalition {
    Neutral = 0,
    Red = 1,
    Blue = 2,
  };
};

}// namespace OpenKneeboard
