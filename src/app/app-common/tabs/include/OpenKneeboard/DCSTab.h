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

#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/TabWithGameEvents.h>

#include <filesystem>

namespace OpenKneeboard {

class DCSTab : public virtual TabWithGameEvents {
 public:
  DCSTab();
  virtual void PostGameEvent(const GameEvent&) override final;

 protected:
  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath)
    = 0;

 private:
  std::filesystem::path mInstallPath;
  std::filesystem::path mSavedGamesPath;
};

}// namespace OpenKneeboard
