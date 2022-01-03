#pragma once

#include "OpenKneeboard/Game.h"

#include <filesystem>
#include <string>

namespace OpenKneeboard::Games {

class DCSWorld final : public OpenKneeboard::Game {
 public:
  const char* GetNameForConfigFile() const override;
  wxString GetUserFriendlyName(const std::filesystem::path&) const override final;
  virtual std::vector<std::filesystem::path> GetInstalledPaths() const override final;
  virtual bool DiscardOculusDepthInformationDefault() const override final;

  static std::filesystem::path GetStablePath();
  static std::filesystem::path GetOpenBetaPath();

  static constexpr char EVT_AIRCRAFT[]
    = "com.fredemmott.openkneeboard.dcsext/Aircraft";
  static constexpr char EVT_INSTALL_PATH[]
    = "com.fredemmott.openkneeboard.dcsext/InstallPath";
  static constexpr char EVT_MISSION[]
    = "com.fredemmott.openkneeboard.dcsext/Mission";
  static constexpr char EVT_RADIO_MESSAGE[]
    = "com.fredemmott.openkneeboard.dcsext/RadioMessage";
  static constexpr char EVT_SAVED_GAMES_PATH[]
    = "com.fredemmott.openkneeboard.dcsext/SavedGamesPath";
  static constexpr char EVT_SIMULATION_START[]
    = "com.fredemmott.openkneeboard.dcsext/SimulationStart";
  static constexpr char EVT_TERRAIN[]
    = "com.fredemmott.openkneeboard.dcsext/Terrain";
};

}// namespace OpenKneeboard::Games
