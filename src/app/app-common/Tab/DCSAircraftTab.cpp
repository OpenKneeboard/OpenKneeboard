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
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/FolderPageSource.h>
#include <OpenKneeboard/GameEvent.h>

#include <OpenKneeboard/dprint.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSAircraftTab::DCSAircraftTab(const DXResources& dxr, KneeboardState* kbs)
  : DCSAircraftTab(dxr, kbs, {}, _("Aircraft")) {
}

DCSAircraftTab::DCSAircraftTab(
  const DXResources& dxr,
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

DCSAircraftTab::~DCSAircraftTab() {
  this->RemoveAllEventListeners();
}

std::string DCSAircraftTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string DCSAircraftTab::GetStaticGlyph() {
  return "\uE709";
}

void DCSAircraftTab::Reload() {
  mPaths = {};
  mAircraft = {};
  this->SetDelegates({});
}

std::string DCSAircraftTab::GetDebugInformation() const {
  return mDebugInformation;
}

void DCSAircraftTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath) {
  if (event.name != DCS::EVT_AIRCRAFT) {
    return;
  }
  if (event.value == mAircraft) {
    return;
  }
  mDebugInformation = _("Looking for files in:");

  mAircraft = event.value;
  auto moduleName = mAircraft;
  if (mAircraft == "FA-18C_hornet") {
    moduleName = "FA-18C";
  } else if (mAircraft == "F-16C_50") {
    moduleName = "F-16C";
  }

  std::vector<std::filesystem::path> paths;

  for (const auto& path: {
         savedGamesPath / "KNEEBOARD" / mAircraft,
         installPath / "Mods" / "aircraft" / moduleName / "Cockpit"
           / "KNEEBOARD" / "pages",
         installPath / "Mods" / "aircraft" / moduleName / "Cockpit" / "Scripts"
           / "KNEEBOARD" / "pages",
       }) {
    if (std::filesystem::exists(path)) {
      paths.push_back(std::filesystem::canonical(path));
      mDebugInformation += std::format("\n\u2714 {}", to_utf8(path));
    } else {
      mDebugInformation += std::format("\n\u274c {}", to_utf8(path));
    }
  }
  dprint("Aircraft tab:" + mDebugInformation);

  if (mPaths == paths) {
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
