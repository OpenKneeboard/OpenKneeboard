// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/LegacyNonVRSettings.hpp>

#include <OpenKneeboard/json/LegacyNonVRSettings.hpp>
#include <OpenKneeboard/json/NonVRConstrainedPosition.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard {

void to_json(nlohmann::json& j, const LegacyNonVRSettings& v) {
  to_json(j, static_cast<const NonVRConstrainedPosition&>(v));
  j["Opacity"] = v.mOpacity;
}
void from_json(const nlohmann::json& j, LegacyNonVRSettings& v) {
  from_json(j, static_cast<NonVRConstrainedPosition&>(v));
  if (j.contains("Opacity")) {
    v.mOpacity = j.at("Opacity");
  }
}

}// namespace OpenKneeboard
