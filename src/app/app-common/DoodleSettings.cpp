#include <OpenKneeboard/DoodleSettings.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

void from_json(const nlohmann::json& j, DoodleSettings& ds) {
  if (j.contains("pen")) {
    auto dj = j.at("pen");
    ds.minimumPenRadius = dj.at("minimumRadius");
    ds.penSensitivity = dj.at("sensitivity");
  }
  if (j.contains("eraser")) {
    auto dj = j.at("eraser");
    ds.minimumEraseRadius = dj.at("minimumRadius");
    ds.eraseSensitivity = dj.at("sensitivity");
  }
}

void to_json(nlohmann::json& j, const DoodleSettings& ds) {
  j = {
    {
      "pen",
      {
        {"minimumRadius", ds.minimumPenRadius},
        {"sensitivity", ds.penSensitivity},
      },
    },
    {
      "eraser",
      {
        {"minimumRadius", ds.minimumEraseRadius},
        {"sensitivity", ds.eraseSensitivity},
      },
    },
  };
}

}// namespace OpenKneeboard
