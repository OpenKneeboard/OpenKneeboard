// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DCSWorld.hpp>

#include <GeographicLib/TransverseMercator.hpp>

namespace OpenKneeboard {

class DCSGrid final {
 public:
  DCSGrid(DCSWorld::GeoReal originLat, DCSWorld::GeoReal originLong);
  std::tuple<DCSWorld::GeoReal, DCSWorld::GeoReal> LatLongFromXY(
    DCSWorld::GeoReal x,
    DCSWorld::GeoReal y) const;

 private:
  DCSWorld::GeoReal mOffsetX;
  DCSWorld::GeoReal mOffsetY;
  DCSWorld::GeoReal mZoneMeridian;
};

}// namespace OpenKneeboard
