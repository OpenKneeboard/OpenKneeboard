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
  : PageSourceWithDelegates(dxr, kbs), mDXR(dxr), mKneeboard(kbs) {
}

DCSAircraftTab::~DCSAircraftTab() {
  this->RemoveAllEventListeners();
}

utf8_string DCSAircraftTab::GetGlyph() const {
  return "\uE709";
}

utf8_string DCSAircraftTab::GetTitle() const {
  return _("Aircraft");
}

void DCSAircraftTab::Reload() {
  mPaths = {};
  mAircraft = {};
  this->SetDelegates({});
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
  mAircraft = event.value;

  std::vector<std::filesystem::path> paths;

  auto path = savedGamesPath / "KNEEBOARD" / event.value;

  dprintf("Aircraft tab: looking for {}", path);
  if (std::filesystem::exists(path)) {
    paths.push_back(std::filesystem::canonical(path));
  }

  // TODO: check for built-in aircraft kneeboard images
  if (mPaths == paths) {
    return;
  }
  mPaths = paths;

  std::vector<std::shared_ptr<IPageSource>> delegates;
  for (auto& path: paths) {
    delegates.push_back(std::static_pointer_cast<IPageSource>(
      std::make_shared<FolderPageSource>(mDXR, mKneeboard, path)));
  }
  this->SetDelegates(delegates);
}

}// namespace OpenKneeboard
