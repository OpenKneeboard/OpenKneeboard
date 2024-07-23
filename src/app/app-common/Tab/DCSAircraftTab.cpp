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
#include <OpenKneeboard/DCSAircraftTab.hpp>
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/FolderPageSource.hpp>
#include <OpenKneeboard/GameEvent.hpp>

#include <OpenKneeboard/dprint.hpp>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSAircraftTab::DCSAircraftTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : DCSAircraftTab(dxr, kbs, {}, _("Aircraft")) {
}

DCSAircraftTab::DCSAircraftTab(
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

DCSAircraftTab::~DCSAircraftTab() {
  this->RemoveAllEventListeners();
}

std::string DCSAircraftTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string DCSAircraftTab::GetStaticGlyph() {
  return "\uE709";
}

winrt::Windows::Foundation::IAsyncAction DCSAircraftTab::Reload() {
  mPaths = {};
  mAircraft = {};
  co_await this->SetDelegates({});
}

std::string DCSAircraftTab::GetDebugInformation() const {
  return mDebugInformation;
}

winrt::fire_and_forget DCSAircraftTab::OnGameEvent(
  GameEvent event,
  std::filesystem::path installPath,
  std::filesystem::path savedGamesPath) {
  if (event.name != DCS::EVT_AIRCRAFT) {
    co_return;
  }
  if (event.value == mAircraft) {
    co_return;
  }

  mAircraft = event.value;
  const auto moduleName = DCSWorld::GetModuleNameForLuaAircraft(mAircraft);

  mDebugInformation = DCSTab::DebugInformationHeader;

  std::vector<std::filesystem::path> paths;

  for (const auto& path: {
         savedGamesPath / "KNEEBOARD" / mAircraft,
         installPath / "Mods" / "aircraft" / moduleName / "Cockpit"
           / "KNEEBOARD" / "pages",
         installPath / "Mods" / "aircraft" / moduleName / "Cockpit" / "Scripts"
           / "KNEEBOARD" / "pages",
       }) {
    std::string message;
    if (std::filesystem::exists(path)) {
      paths.push_back(std::filesystem::canonical(path));
      message = "\u2714 " + to_utf8(path);
    } else {
      message = "\u274c " + to_utf8(path);
    }
    dprintf("Aircraft tab: {}", message);
    mDebugInformation += "\n" + message;
  }

  evDebugInformationHasChanged.Emit(mDebugInformation);

  if (mPaths == paths) {
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
