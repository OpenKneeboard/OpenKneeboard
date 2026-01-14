// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Coordinates.hpp>
#include <OpenKneeboard/DCSEvents.hpp>

#include <GeographicLib/TransverseMercator.hpp>

namespace OpenKneeboard {

class DCSGrid final {
 public:
  using GeoReal = DCSEvents::GeoReal;
  static_assert(std::same_as<OpenKneeboard::GeoReal, DCSGrid::GeoReal>);

  DCSGrid() = delete;
  DCSGrid(GeoReal originLat, GeoReal originLong);
  std::tuple<GeoReal, GeoReal> LatLongFromXY(GeoReal x, GeoReal y) const;

 private:
  GeoReal mOffsetX;
  GeoReal mOffsetY;
  GeoReal mZoneMeridian;
};

}// namespace OpenKneeboard
