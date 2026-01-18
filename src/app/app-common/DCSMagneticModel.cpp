// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/DCSMagneticModel.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <felly/numeric_cast.hpp>
#include <felly/unique_any.hpp>

using felly::numeric_cast;

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
  using unique_magmodel_ptr
    = felly::unique_any<MAGtype_MagneticModel*, &MAG_FreeMagneticModelMemory>;

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

  return numeric_cast<float>(geoElements.Decl);
}

}// namespace OpenKneeboard
