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

#include <OpenKneeboard/Alignment.h>
#include <OpenKneeboard/Pixels.h>

#include <compare>
#include <cstdint>
#include <numbers>

namespace OpenKneeboard {

struct NonVRAbsolutePosition {
  PixelRect mRect {};
  Alignment::Horizontal mHorizontalAlignment {Alignment::Horizontal::Right};
  Alignment::Vertical mVerticalAlignment {Alignment::Vertical::Middle};
};
static_assert(std::is_standard_layout_v<NonVRAbsolutePosition>);

struct NonVRConstrainedPosition {
  uint8_t mHeightPercent = 60;
  uint32_t mPaddingPixels = 10;
  // In case it covers up menus etc
  float mOpacity = 0.8f;

  Alignment::Horizontal mHorizontalAlignment = Alignment::Horizontal::Right;
  Alignment::Vertical mVerticalAlignment = Alignment::Vertical::Middle;

  PixelRect Layout(PixelSize canvasSize, PixelSize imageSize) const;

  constexpr auto operator<=>(const NonVRConstrainedPosition&) const = default;
};
static_assert(std::is_standard_layout_v<NonVRConstrainedPosition>);

#ifdef OPENKNEEBOARD_JSON_SERIALIZE
void to_json(nlohmann::json& j, const NonVRAbsolutePosition&);
void from_json(const nlohmann::json& j, NonVRAbsolutePosition&);
OPENKNEEBOARD_DECLARE_SPARSE_JSON(NonVRConstrainedPosition);
#endif

}// namespace OpenKneeboard
