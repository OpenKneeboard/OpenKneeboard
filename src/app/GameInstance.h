#pragma once

#include <memory>
#include "OpenKneeboard/Game.h"

namespace OpenKneeboard {
  struct GameInstance {
    std::filesystem::path Path;
    std::shared_ptr<Game> Game;
  };
};
