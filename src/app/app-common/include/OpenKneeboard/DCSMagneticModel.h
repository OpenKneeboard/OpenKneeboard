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

#include <chrono>
#include <shims/filesystem>
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
