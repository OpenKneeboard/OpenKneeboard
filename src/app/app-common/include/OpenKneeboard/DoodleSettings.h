#pragma once

#include <nlohmann/json_fwd.hpp>

namespace OpenKneeboard {
struct DoodleSettings final {
  uint32_t minimumPenRadius = 1;
  uint32_t minimumEraseRadius = 10;
  uint32_t penSensitivity = 15;
  uint32_t eraseSensitivity = 150;
  // TODO: stroke color?
};

void from_json(const nlohmann::json&, DoodleSettings&);
void to_json(nlohmann::json&, const DoodleSettings&);

}// namespace OpenKneeboard
