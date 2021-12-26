#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace OpenKneeboard {
  struct GameEvent final {
    std::string Name;
    std::string Value;

    operator bool() const;

    static GameEvent Unserialize(const std::vector<std::byte>& payload);
    std::vector<std::byte> Serialize() const;
  };
}
