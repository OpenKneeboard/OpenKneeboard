// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/json_fwd.hpp>

namespace OpenKneeboard {

struct LegacyNonVRSettings;

void to_json(nlohmann::json& j, const LegacyNonVRSettings&);
void from_json(const nlohmann::json& j, LegacyNonVRSettings&);

}// namespace OpenKneeboard
