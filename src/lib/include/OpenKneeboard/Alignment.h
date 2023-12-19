/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <cstdint>
#ifdef OPENKNEEBOARD_JSON_SERIALIZE
#include <OpenKneeboard/json.h>
#endif

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

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
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
#endif
}// namespace OpenKneeboard::Alignment
