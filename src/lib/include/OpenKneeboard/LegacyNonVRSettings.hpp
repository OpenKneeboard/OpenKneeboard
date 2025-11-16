// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/NonVRConstrainedPosition.hpp>

namespace OpenKneeboard {

// Replaced by `ViewSettings` in v1.7+
struct LegacyNonVRSettings : NonVRConstrainedPosition {
  // In case it covers up menus etc
  float mOpacity = 0.8f;
  constexpr bool operator==(const LegacyNonVRSettings&) const = default;
};

}// namespace OpenKneeboard
