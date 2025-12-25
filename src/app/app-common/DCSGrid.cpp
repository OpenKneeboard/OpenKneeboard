// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/DCSGrid.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <GeographicLib/UTMUPS.hpp>

namespace {
const auto& UTM() {
  static auto value = GeographicLib::TransverseMercator::UTM();
  return value;
}
}// namespace
namespace OpenKneeboard {

static_assert(std::is_same_v<GeographicLib::Math::real, DCSWorld::GeoReal>);

DCSGrid::DCSGrid(DCSWorld::GeoReal originLat, DCSWorld::GeoReal originLong) {
  const int zone = GeographicLib::UTMUPS::StandardZone(originLat, originLong);
  mZoneMeridian = (6.0 * zone - 183);

  UTM().Forward(mZoneMeridian, originLat, originLong, mOffsetX, mOffsetY);

  dprint(
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

  UTM().Reverse(mZoneMeridian, x, y, retLat, retLong);
  return {retLat, retLong};
}

}// namespace OpenKneeboard
