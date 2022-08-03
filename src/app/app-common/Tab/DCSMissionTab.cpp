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
#include <OpenKneeboard/DCSExtractedMission.h>
#include <OpenKneeboard/DCSMissionTab.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSMissionTab::DCSMissionTab(const DXResources& dxr, KneeboardState* kbs)
  : TabWithDelegate(
    std::make_shared<FolderTab>(dxr, kbs, "", std::filesystem::path {})) {
}

DCSMissionTab::~DCSMissionTab() {
  GetDelegate()->SetPath({});
}

utf8_string DCSMissionTab::GetGlyph() const {
  return "\uF0E3";
}

utf8_string DCSMissionTab::GetTitle() const {
  return _("Mission");
}

void DCSMissionTab::Reload() {
  if ((!mExtracted) || mExtracted->GetZipPath() != mMission) {
    mExtracted = DCSExtractedMission::Get(mMission);
  }

  dprintf("Mission tab: loading {}", mMission);

  const auto root = mExtracted->GetExtractedPath();

  if (!mAircraft.empty()) {
    const std::filesystem::path aircraftPath {
      root / "KNEEBOARD" / mAircraft / "IMAGES"};
    dprintf("Checking {}", aircraftPath);
    if (std::filesystem::exists(aircraftPath)) {
      dprint("Using aircraft-specific path");
      GetDelegate()->SetPath(aircraftPath);
      return;
    }
  } else {
    dprint("Aircraft not detected.");
  }

  const std::filesystem::path genericPath {root / "KNEEBOARD" / "IMAGES"};
  dprintf("Using {}", genericPath);
  GetDelegate()->SetPath(genericPath);
}

void DCSMissionTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& _installPath,
  const std::filesystem::path& _savedGamePath) {
  if (event.name == DCS::EVT_MISSION) {
    auto mission = std::filesystem::canonical(event.value);
    if (mission == mMission) {
      return;
    }
    mMission = mission;
  } else if (event.name == DCS::EVT_AIRCRAFT) {
    if (event.value == mAircraft) {
      return;
    }
    mAircraft = event.value;
  } else {
    return;
  }

  this->Reload();
}

}// namespace OpenKneeboard
