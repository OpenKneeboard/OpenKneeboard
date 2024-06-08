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
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>

#include <OpenKneeboard/utf8.h>

#include <shims/filesystem>

namespace OpenKneeboard {

struct GameEvent;

class DCSTab : public virtual ITab, public virtual EventReceiver {
 public:
  DCSTab(KneeboardState*);
  virtual ~DCSTab();

  DCSTab() = delete;

 protected:
  static constexpr std::string_view DebugInformationHeader = _(
    "A tick or a cross indicates whether or not the folder exists, not whether "
    "or not it is meant to exist. Some crosses are expected, and not "
    "necessarily an error.\n");

  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath)
    = 0;

  std::filesystem::path ToAbsolutePath(const std::filesystem::path&);

 private:
  std::filesystem::path mInstallPath;
  std::filesystem::path mSavedGamesPath;
  EventHandlerToken mGameEventToken;

  void OnGameEvent(const GameEvent&);
};

}// namespace OpenKneeboard
