#pragma once

#include <nlohmann/json_fwd.hpp>

namespace OpenKneeboard {
struct DoodleSettings final {
  uint32_t drawRadius = 50;
  uint32_t eraseRadius = 150;
  bool fixedDraw = false;
  bool fixedErase = true;
  // TODO: stroke color?
};

void from_json(const nlohmann::json&, DoodleSettings&);
void to_json(nlohmann::json&, const DoodleSettings&);

}// namespace OpenKneeboard