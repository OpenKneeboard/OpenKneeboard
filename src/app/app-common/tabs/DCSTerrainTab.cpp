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
#include <OpenKneeboard/DCSTerrainTab.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>

#include <shims/filesystem>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab(const DXResources& dxr, KneeboardState* kbs)
  : TabWithDelegate(
    std::make_shared<FolderTab>(dxr, kbs, "", std::filesystem::path {})) {
}

utf8_string DCSTerrainTab::GetGlyph() const {
  return "\uE909";
}

utf8_string DCSTerrainTab::GetTitle() const {
  return _("Theater");
}

void DCSTerrainTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path& _savedGamesPath) {
  if (event.name != DCS::EVT_TERRAIN) {
    return;
  }

  const auto path
    = installPath / "Mods" / "terrains" / event.value / "Kneeboard";
  dprintf("Terrain tab: loading {}", path);
  this->GetDelegate()->SetPath(path);
}

}// namespace OpenKneeboard
