#pragma once

#include <string>

namespace OpenKneeboard::Games {

class DCSWorld final {
 public:
  static std::string GetStablePath();
  static std::string GetOpenBetaPath();
  static std::string GetNewestPath();

  static constexpr char EVT_AIRCRAFT[] = "com.fredemmott.openkneeboard.dcsext/Aircraft";
  static constexpr char EVT_MISSION[] = "com.fredemmott.openkneeboard.dcsext/Mission";
  static constexpr char EVT_TERRAIN[] = "com.fredemmott.openkneeboard.dcsext/Terrain";
};

}// namespace OpenKneeboard::Games
