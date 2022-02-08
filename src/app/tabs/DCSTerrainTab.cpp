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
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/Games/DCSWorld.h>
#include <OpenKneeboard/dprint.h>

#include <filesystem>

#include "okEvents.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab(const DXResources& dxr)
  : DCSTab(dxr, _("Theater")),
    mDelegate(
      std::make_shared<FolderTab>(dxr, wxString {}, std::filesystem::path {})) {
  AddEventListener(mDelegate->evFullyReplacedEvent, this->evFullyReplacedEvent);
}

DCSTerrainTab::~DCSTerrainTab() {
}

void DCSTerrainTab::Reload() {
  mDelegate->Reload();
}

uint16_t DCSTerrainTab::GetPageCount() const {
  return mDelegate->GetPageCount();
}

void DCSTerrainTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  mDelegate->RenderPage(ctx, pageIndex, rect);
}

D2D1_SIZE_U DCSTerrainTab::GetNativeContentSize(uint16_t pageIndex) {
  return mDelegate->GetNativeContentSize(pageIndex);
}

const char* DCSTerrainTab::GetGameEventName() const {
  return DCS::EVT_TERRAIN;
}

void DCSTerrainTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& _savedGamesPath,
  const std::string& value) {
  auto path = installPath / "Mods" / "terrains" / value / "Kneeboard";
  mDelegate->SetPath(path);
}

std::shared_ptr<Tab> DCSTerrainTab::CreateNavigationTab(uint16_t page) {
  return mDelegate->CreateNavigationTab(page);
}

}// namespace OpenKneeboard
