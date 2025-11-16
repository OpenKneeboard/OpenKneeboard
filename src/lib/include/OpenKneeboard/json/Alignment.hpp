// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Alignment.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard::Alignment {

NLOHMANN_JSON_SERIALIZE_ENUM(
  Horizontal,
  {
    {Horizontal::Left, "Left"},
    {Horizontal::Center, "Center"},
    {Horizontal::Right, "Right"},
  });

NLOHMANN_JSON_SERIALIZE_ENUM(
  Vertical,
  {
    {Vertical::Top, "Top"},
    {Vertical::Middle, "Middle"},
    {Vertical::Bottom, "Bottom"},
  });

}// namespace OpenKneeboard::Alignment