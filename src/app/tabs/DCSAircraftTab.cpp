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
  : TabWithDelegate(
    std::make_shared<FolderTab>(dxr, wxString {}, std::filesystem::path {})) {
}

std::wstring DCSAircraftTab::GetTitle() const {
  static std::wstring sCache;
  if (!sCache.empty()) [[likely]] {
    return sCache;
  }
  sCache = _("Aircraft").ToStdWstring();
  return sCache;
}

const char* DCSAircraftTab::GetGameEventName() const {
  return DCS::EVT_AIRCRAFT;
}

void DCSAircraftTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath,
  const std::string& aircraft) {
  auto path = savedGamesPath / "KNEEBOARD" / aircraft;
  this->GetDelegate()->SetPath(path);
}

}// namespace OpenKneeboard
