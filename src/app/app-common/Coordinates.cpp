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

#include <OpenKneeboard/Coordinates.h>

#include <GeographicLib/DMS.hpp>
#include <GeographicLib/GeoCoords.hpp>

#include <format>

namespace OpenKneeboard::Coordinates {

static_assert(
  std::is_same_v<GeographicLib::Math::real, OpenKneeboard::GeoReal>);

std::string DMSFormat(GeoReal angle, char pos, char neg) {
  GeoReal degrees {}, minutes {}, seconds {};
  GeographicLib::DMS::Encode(angle, degrees, minutes, seconds);
  return std::format(
    "{} {:03.0f}°{:02.0f}'{:05.2f}\"",
    degrees >= 0 ? pos : neg,
    std::abs(degrees),
    minutes,
    seconds);
};

std::string DMFormat(GeoReal angle, char pos, char neg) {
  GeoReal degrees {}, minutes {};
  GeographicLib::DMS::Encode(angle, degrees, minutes);
  return std::format(
    "{} {:03.0f}°{:05.3f}'",
    degrees >= 0 ? pos : neg,
    std::abs(degrees),
    minutes);
}

std::string MGRSFormat(GeoReal latitude, GeoReal longitude) {
  const auto raw
    = GeographicLib::GeoCoords(latitude, longitude).MGRSRepresentation(0);
  // e.g. 37TEHnnnnneeeee
  //                ^ -5
  //           ^ -10
  //         ^-12
  const std::string_view view(raw);
  return std::format(
    "{} {} {} {}",
    view.substr(0, view.size() - 12),
    view.substr(view.size() - 12, 2),
    view.substr(view.size() - 10, 5),
    view.substr(view.size() - 5, 5));
}

}// namespace OpenKneeboard::Coordinates
