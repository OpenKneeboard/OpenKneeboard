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

#include <OpenKneeboard/DCSMagneticModel.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/handles.hpp>

namespace OpenKneeboard {

DCSMagneticModel::DCSMagneticModel(
  const std::filesystem::path& dcsInstallation) {
  const auto cofDir = dcsInstallation / "Data" / "MagVar" / "COF";

  for (const auto& file: std::filesystem::directory_iterator(cofDir)) {
    if (!std::filesystem::is_regular_file(file)) {
      continue;
    }
    MAGtype_MagneticModel* model = nullptr;
    MAG_robustReadMagModels(
      const_cast<char*>(file.path().string().c_str()),
      reinterpret_cast<MAGtype_MagneticModel*(*)[]>(&model),
      1);
    mModels.push_back(model);
  }
}

DCSMagneticModel::~DCSMagneticModel() {
  for (auto model: mModels) {
    MAG_FreeMagneticModelMemory(model);
  }
}

MAGtype_MagneticModel* DCSMagneticModel::GetModel(
  const std::chrono::year_month_day& date) const {
  MAGtype_Date magDate {
    static_cast<int>(date.year()),
    static_cast<int>(static_cast<unsigned>(date.month())),
    static_cast<int>(static_cast<unsigned>(date.day())),
  };
  char error[512];
  MAG_DateToYear(&magDate, error);
  const auto year = magDate.DecimalYear;

  for (auto model: mModels) {
    if (year < model->epoch) {
      dprint(
        "No WMM model for historical year {}, using incorrect {:.0f} model",
        date.year(),
        model->epoch);
      return model;
    }

    if (year - model->epoch <= 5) {
      dprint(
        "Using correct WMM {:0.0f} model for year {}",
        model->epoch,
        date.year());
      return model;
    }
  }

  auto model = mModels.back();

  dprint(
    "No WMM model found for future year {}, using incorrect {:.0f} model",
    date.year(),
    model->epoch);
  return model;
}

float DCSMagneticModel::GetMagneticVariation(
  const std::chrono::year_month_day& date,
  float latitude,
  float longitude) const {
  const MAGtype_CoordGeodetic geoCoord {
    .lambda = longitude,
    .phi = latitude,
  };

  MAGtype_Ellipsoid ellipsoid;
  MAGtype_Geoid geoid;
  MAG_SetDefaults(&ellipsoid, &geoid);
  MAGtype_CoordSpherical sphereCoord {};
  MAG_GeodeticToSpherical(ellipsoid, geoCoord, &sphereCoord);

  MAGtype_MagneticModel* model = this->GetModel(date);
  using unique_magmodel_ptr = std::unique_ptr<
    MAGtype_MagneticModel,
    CPtrDeleter<MAGtype_MagneticModel, &MAG_FreeMagneticModelMemory>>;

  // Taken from wmm_point.c sample
  const auto nMax = model->nMax;
  const unique_magmodel_ptr timedModel {
    MAG_AllocateModelMemory((nMax + 1) * (nMax + 2) / 2)};

  MAGtype_Date magDate {
    static_cast<int>(date.year()),
    static_cast<int>(static_cast<unsigned>(date.month())),
    static_cast<int>(static_cast<unsigned>(date.day())),
  };
  char error[512];
  MAG_DateToYear(&magDate, error);
  MAG_TimelyModifyMagneticModel(magDate, model, timedModel.get());

  MAGtype_GeoMagneticElements geoElements {};
  MAG_Geomag(ellipsoid, sphereCoord, geoCoord, timedModel.get(), &geoElements);

  return geoElements.Decl;
}

}// namespace OpenKneeboard
