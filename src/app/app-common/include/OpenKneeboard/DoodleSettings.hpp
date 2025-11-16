// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/json_fwd.hpp>

namespace OpenKneeboard {
struct DoodleSettings final {
  struct Tool {
    uint32_t mMinimumRadius;
    uint32_t mSensitivity;

    constexpr bool operator==(const Tool&) const = default;
  };

  Tool mPen {
    .mMinimumRadius = 1,
    .mSensitivity = 15,
  };
  Tool mEraser {
    .mMinimumRadius = 10,
    .mSensitivity = 150,
  };

  constexpr bool operator==(const DoodleSettings&) const = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(DoodleSettings)

}// namespace OpenKneeboard
