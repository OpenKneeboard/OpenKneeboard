// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <cstdint>

namespace OpenKneeboard::Alignment {

enum class Horizontal : uint8_t {
  Left,
  Center,
  Right,
};

enum class Vertical : uint8_t {
  Top,
  Middle,
  Bottom,
};

}// namespace OpenKneeboard::Alignment
