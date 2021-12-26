#pragma once

#include <string>

namespace OpenKneeboard::Games {

class DCSWorld final {
 public:
  static std::string GetStablePath();
  static std::string GetOpenBetaPath();
};

}// namespace OpenKneeboard::Games
