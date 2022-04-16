#pragma once

#include <nlohmann/json.hpp>

namespace OpenKneeboard {
struct DoodleSettings final {
  uint32_t DrawRadius = 50;
  uint32_t EraseRadius = 150;
  bool FixedDraw = false;
  bool FixedErase = true;
  // TODO: stroke colour?
};

void from_json(const nlohmann::json&, DoodleSettings&);
void to_json(nlohmann::json&, const DoodleSettings&);

}// namespace OpenKneeboard