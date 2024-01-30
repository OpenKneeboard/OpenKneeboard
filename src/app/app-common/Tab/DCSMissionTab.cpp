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
#include <OpenKneeboard/FolderPageSource.h>
#include <OpenKneeboard/GameEvent.h>

#include <OpenKneeboard/dprint.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSMissionTab::DCSMissionTab(const audited_ptr<DXResources>& dxr, KneeboardState* kbs)
  : DCSMissionTab(dxr, kbs, {}, _("Mission")) {
}

DCSMissionTab::DCSMissionTab(
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

DCSMissionTab::~DCSMissionTab() {
  this->RemoveAllEventListeners();
}

std::string DCSMissionTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string DCSMissionTab::GetStaticGlyph() {
  return "\uF0E3";
}

void DCSMissionTab::Reload() {
  if (mMission.empty()) {
    return;
  }

  // Free the filesystem watchers etc before potentially
  // deleting the extracted mission
  this->SetDelegates({});

  if ((!mExtracted) || mExtracted->GetZipPath() != mMission) {
    mExtracted = DCSExtractedMission::Get(mMission);
  }

  mDebugInformation = to_utf8(mMission) + "\n";

  const auto root = mExtracted->GetExtractedPath();

  std::vector<std::filesystem::path> paths {
    std::filesystem::path("KNEEBOARD") / "IMAGES",
  };

  if (!mAircraft.empty()) {
    paths.push_back(std::filesystem::path("KNEEBOARD") / mAircraft / "IMAGES");
  }

  std::vector<std::shared_ptr<IPageSource>> sources;

  for (const auto& path: paths) {
    if (std::filesystem::exists(root / path)) {
      sources.push_back(
        FolderPageSource::Create(mDXR, mKneeboard, root / path));
      mDebugInformation += std::format("\u2714 miz:\\{}\n", to_utf8(path));
    } else {
      mDebugInformation += std::format("\u274c miz:\\{}\n", to_utf8(path));
    }
  }

  if (mDebugInformation.ends_with('\n')) {
    mDebugInformation.pop_back();
  }

  dprint("Mission tab: " + mDebugInformation);
  evDebugInformationHasChanged.Emit(mDebugInformation);
  this->SetDelegates(sources);
}

std::string DCSMissionTab::GetDebugInformation() const {
  return mDebugInformation;
}

void DCSMissionTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& _installPath,
  const std::filesystem::path& _savedGamePath) {
  if (event.name == DCS::EVT_MISSION) {
    const auto missionZip = this->ToAbsolutePath(event.value);
    if (missionZip.empty() || !std::filesystem::exists(missionZip)) {
      dprintf("MissionTab: mission '{}' does not exist", event.value);
      return;
    }

    if (missionZip == mMission) {
      return;
    }

    mMission = missionZip;
    this->Reload();
    return;
  }

  if (event.name == DCS::EVT_AIRCRAFT) {
    if (event.value == mAircraft) {
      return;
    }
    mAircraft = event.value;
    this->Reload();
    return;
  }
}

}// namespace OpenKneeboard
