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
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/DCSTerrainTab.hpp>
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/FolderPageSource.hpp>

#include <OpenKneeboard/dprint.hpp>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : DCSTerrainTab(dxr, kbs, {}, _("Theater")) {
}

DCSTerrainTab::DCSTerrainTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title),
    DCSTab(kbs),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mDebugInformation(_("No data from DCS.")) {
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

std::string DCSTerrainTab::GetDebugInformation() const {
  return mDebugInformation;
}

winrt::Windows::Foundation::IAsyncAction DCSTerrainTab::Reload() {
  mPaths = {};
  mTerrain = {};
  co_await this->SetDelegates({});
}

static std::string_view NormalizeTerrain(std::string_view terrain) {
  for (const auto suffix:
       {std::string_view {"Map"},
        std::string_view {"Terrain"},
        std::string_view {"Theater"}}) {
    if (terrain.ends_with(suffix)) {
      return terrain.substr(0, terrain.length() - suffix.length());
    }
  }
  return terrain;
}

winrt::fire_and_forget DCSTerrainTab::OnAPIEvent(
  APIEvent event,
  std::filesystem::path installPath,
  std::filesystem::path savedGamesPath) {
  if (event.name != DCS::EVT_TERRAIN) {
    co_return;
  }
  if (event.value == mTerrain) {
    co_return;
  }
  mTerrain = event.value;
  const auto normalized = NormalizeTerrain(event.value);

  mDebugInformation = DCSTab::DebugInformationHeader;

  std::vector<std::filesystem::path> potentialPaths {
    savedGamesPath / "KNEEBOARD" / mTerrain,
    savedGamesPath / "KNEEBOARD" / normalized,
    installPath / "Mods" / "terrains" / mTerrain / "Kneeboard",
    installPath / "Mods" / "terrains" / normalized / "Kneeboard",
  };
  {
    const auto last = std::unique(potentialPaths.begin(), potentialPaths.end());
    potentialPaths.erase(last, potentialPaths.end());
  }

  std::vector<std::filesystem::path> paths;

  for (auto path: potentialPaths) {
    std::string message;
    if (std::filesystem::exists(path)) {
      paths.push_back(std::filesystem::canonical(path));
      message = std::format("\u2714 {}", to_utf8(path));
    } else {
      message = std::format("\u274c {}", to_utf8(path));
    }
    dprintf("Terrain tab: {}", message);
    mDebugInformation += "\n" + message;
  }

  evDebugInformationHasChanged.Emit(mDebugInformation);

  if (paths == mPaths) {
    co_return;
  }
  mPaths = paths;

  std::vector<std::shared_ptr<IPageSource>> delegates;
  for (auto& path: paths) {
    delegates.push_back(std::static_pointer_cast<IPageSource>(
      FolderPageSource::Create(mDXR, mKneeboard, path)));
  }
  co_await this->SetDelegates(delegates);
}

}// namespace OpenKneeboard
