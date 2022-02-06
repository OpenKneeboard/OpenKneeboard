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
#include <OpenKneeboard/Games/DCSWorld.h>
#include <OpenKneeboard/dprint.h>

#include <filesystem>

#include "okEvents.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSAircraftTab::DCSAircraftTab(const DXResources& dxr)
  : DCSTab(dxr, _("Aircraft")),
    mDelegate(
      std::make_shared<FolderTab>(dxr, wxString {}, std::filesystem::path {})) {
  AddEventListener(mDelegate->evFullyReplacedEvent, this->evFullyReplacedEvent);
}

DCSAircraftTab::~DCSAircraftTab() {
}

void DCSAircraftTab::Reload() {
  mDelegate->Reload();
}

uint16_t DCSAircraftTab::GetPageCount() const {
  return mDelegate->GetPageCount();
}

void DCSAircraftTab::RenderPageContent(
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  mDelegate->RenderPage(pageIndex, rect);
}

D2D1_SIZE_U DCSAircraftTab::GetNativeContentSize(uint16_t pageIndex) {
  return mDelegate->GetNativeContentSize(pageIndex);
}

const char* DCSAircraftTab::GetGameEventName() const {
  return DCS::EVT_AIRCRAFT;
}

void DCSAircraftTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath,
  const std::string& aircraft) {
  auto path = savedGamesPath / "KNEEBOARD" / aircraft;
  mDelegate->SetPath(path);
}

}// namespace OpenKneeboard
