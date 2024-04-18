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

#include <OpenKneeboard/DCSGrid.h>

#include <OpenKneeboard/dprint.h>

#include <GeographicLib/UTMUPS.hpp>

namespace OpenKneeboard {

static_assert(std::is_same_v<GeographicLib::Math::real, DCSWorld::GeoReal>);

GeographicLib::TransverseMercator DCSGrid::sModel
  = GeographicLib::TransverseMercator::UTM();

DCSGrid::DCSGrid(DCSWorld::GeoReal originLat, DCSWorld::GeoReal originLong) {
  const int zone = GeographicLib::UTMUPS::StandardZone(originLat, originLong);
  mZoneMeridian = (6.0 * zone - 183);

  sModel.Forward(mZoneMeridian, originLat, originLong, mOffsetX, mOffsetY);

  dprintf(
    "DCS (0, 0) is in UTM zone {}, with meridian at {} and a UTM offset of "
    "({}, {})",
    zone,
    mZoneMeridian,
    mOffsetX,
    mOffsetY);
}

std::tuple<DCSWorld::GeoReal, DCSWorld::GeoReal> DCSGrid::LatLongFromXY(
  DCSWorld::GeoReal dcsX,
  DCSWorld::GeoReal dcsY) const {
  // UTM (x, y) are (easting, northing), but DCS (x, y) are (northing, easting)
  const auto x = mOffsetX + dcsY;
  const auto y = mOffsetY + dcsX;
  DCSWorld::GeoReal retLat {}, retLong {};

  sModel.Reverse(mZoneMeridian, x, y, retLat, retLong);
  return {retLat, retLong};
}

}// namespace OpenKneeboard
