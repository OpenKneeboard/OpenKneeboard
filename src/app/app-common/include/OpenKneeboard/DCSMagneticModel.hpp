// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <chrono>
#include <filesystem>
#include <vector>

extern "C" {
#include <GeomagnetismHeader.h>
}

namespace OpenKneeboard {

class DCSMagneticModel {
 public:
  DCSMagneticModel(const std::filesystem::path& dcsInstallation);
  ~DCSMagneticModel();

  float GetMagneticVariation(
    const std::chrono::year_month_day& date,
    float latitude,
    float longitude) const;

 private:
  MAGtype_MagneticModel* GetModel(
    const std::chrono::year_month_day& date) const;
  std::vector<MAGtype_MagneticModel*> mModels;
};

}// namespace OpenKneeboard
