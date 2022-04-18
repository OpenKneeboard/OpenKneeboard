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
#include <OpenKneeboard/DCSTab.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSTab::DCSTab() {
}

void DCSTab::PostGameEvent(const GameEvent& event) {
  if (event.name == DCS::EVT_INSTALL_PATH) {
    mInstallPath = std::filesystem::canonical(event.value);
  }

  if (event.name == DCS::EVT_SAVED_GAMES_PATH) {
    mSavedGamesPath = std::filesystem::canonical(event.value);
  }

  if (!(mInstallPath.empty() || mSavedGamesPath.empty())) {
    OnGameEvent(event, mInstallPath, mSavedGamesPath);
  }
}

}// namespace OpenKneeboard
