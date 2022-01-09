#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace OpenKneeboard {
struct GameEvent final {
  std::string name;
  std::string value;

  operator bool() const;

  static GameEvent Unserialize(const std::vector<std::byte>& payload);
  std::vector<std::byte> Serialize() const;
  void Send() const;
};
}// namespace OpenKneeboard
