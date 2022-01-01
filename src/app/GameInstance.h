#pragma once

#include <memory>
#include "OpenKneeboard/Game.h"

namespace OpenKneeboard {
  struct GameInstance {
    std::string Name;
    std::filesystem::path Path;
    std::shared_ptr<Game> Game;
  };
};
