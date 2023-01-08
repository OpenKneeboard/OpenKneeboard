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
#include <OpenKneeboard/FolderPageSource.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab(const DXResources& dxr, KneeboardState* kbs)
  : DCSTerrainTab(dxr, kbs, {}, _("Theater")) {
}

DCSTerrainTab::DCSTerrainTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title),
    DCSTab(kbs),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs) {
}

DCSTerrainTab::~DCSTerrainTab() {
  this->RemoveAllEventListeners();
}

std::string DCSTerrainTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string DCSTerrainTab::GetStaticGlyph() {
  return "\uE909";
}

void DCSTerrainTab::Reload() {
  mPaths = {};
  mTerrain = {};
  this->SetDelegates({});
}

void DCSTerrainTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath) {
  if (event.name != DCS::EVT_TERRAIN) {
    return;
  }
  if (event.value == mTerrain) {
    return;
  }
  mTerrain = event.value;

  std::vector<std::filesystem::path> paths;

  for (auto path: {
         savedGamesPath / "KNEEBOARD" / mTerrain,
         installPath / "Mods" / "terrains" / event.value / "Kneeboard",
       }) {
    dprintf("Terrain tab: checking {}", path);
    if (std::filesystem::exists(path)) {
      paths.push_back(std::filesystem::canonical(path));
    }
  }

  if (paths == mPaths) {
    return;
  }
  mPaths = paths;

  std::vector<std::shared_ptr<IPageSource>> delegates;
  for (auto& path: paths) {
    delegates.push_back(std::static_pointer_cast<IPageSource>(
      FolderPageSource::Create(mDXR, mKneeboard, path)));
  }
  this->SetDelegates(delegates);
}

}// namespace OpenKneeboard
