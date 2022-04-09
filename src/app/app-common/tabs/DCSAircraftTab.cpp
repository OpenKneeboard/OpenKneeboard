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
#include <OpenKneeboard/DCSAircraftTab.h>
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/dprint.h>

#include <filesystem>


using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSAircraftTab::DCSAircraftTab(const DXResources& dxr)
  : TabWithDelegate(
    std::make_shared<FolderTab>(dxr, "", std::filesystem::path {})) {
}

utf8_string DCSAircraftTab::GetTitle() const {
  return _("Aircraft");
}

void DCSAircraftTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath) {
  if (event.name != DCS::EVT_AIRCRAFT) {
    return;
  }

  auto path = savedGamesPath / "KNEEBOARD" / event.value;
  this->GetDelegate()->SetPath(path);
}

}// namespace OpenKneeboard
