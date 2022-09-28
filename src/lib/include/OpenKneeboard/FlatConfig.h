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

// Don't include in OpenKneeboard-SHM library
#ifdef OPENKNEEBOARD_JSON_SERIALIZE
#include <OpenKneeboard/json_fwd.h>
#endif

#include <cstdint>
#include <numbers>

namespace OpenKneeboard {

#pragma pack(push)
struct FlatConfig {
  enum class HorizontalAlignment : uint8_t {
    Left,
    Center,
    Right,
  };
  enum class VerticalAlignment : uint8_t {
    Top,
    Middle,
    Bottom,
  };

  uint8_t mHeightPercent = 60;
  uint32_t mPaddingPixels = 10;
  // In case it covers up menus etc
  float mOpacity = 0.8f;

  HorizontalAlignment mHorizontalAlignment = HorizontalAlignment::Right;
  VerticalAlignment mVerticalAlignment = VerticalAlignment::Middle;

  constexpr auto operator<=>(const FlatConfig&) const = default;
};
#pragma pack(pop)

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
OPENKNEEBOARD_DECLARE_SPARSE_JSON(FlatConfig);
#endif

}// namespace OpenKneeboard
