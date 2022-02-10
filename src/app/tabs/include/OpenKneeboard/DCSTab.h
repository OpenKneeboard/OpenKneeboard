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

#include <OpenKneeboard/TabWithGameEvents.h>

#include <filesystem>

namespace OpenKneeboard {

class DCSTab : public virtual TabWithGameEvents {
 public:
  DCSTab();
  virtual void PostGameEvent(const GameEvent&) override;

 protected:
  virtual const char* GetGameEventName() const = 0;
  virtual void Update(
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath,
    utf8_string_view value)
    = 0;

  virtual void OnSimulationStart();

 private:
  struct Config final {
    std::filesystem::path mInstallPath;
    std::filesystem::path mSavedGamesPath;
    utf8_string mValue;
    bool operator==(const Config&) const = default;
  };

  Config mCurrentConfig;
  Config mLastValidConfig;

  std::string mLastValue;

  void Update();
};

}// namespace OpenKneeboard
