#pragma once

#include <filesystem>
#include <string>

namespace OpenKneeboard::Games {

class DCSWorld final {
 public:
  static std::filesystem::path GetStablePath();
  static std::filesystem::path GetOpenBetaPath();

  static constexpr char EVT_AIRCRAFT[]
    = "com.fredemmott.openkneeboard.dcsext/Aircraft";
  static constexpr char EVT_INSTALL_PATH[]
    = "com.fredemmott.openkneeboard.dcsext/InstallPath";
  static constexpr char EVT_MISSION[]
    = "com.fredemmott.openkneeboard.dcsext/Mission";
  static constexpr char EVT_SAVED_GAMES_PATH[]
    = "com.fredemmott.openkneeboard.dcsext/SavedGamesPath";
  static constexpr char EVT_TERRAIN[]
    = "com.fredemmott.openkneeboard.dcsext/Terrain";
};

}// namespace OpenKneeboard::Games
