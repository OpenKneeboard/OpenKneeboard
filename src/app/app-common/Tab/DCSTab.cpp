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
#include <OpenKneeboard/DCSTab.hpp>
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/GameEvent.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/dprint.hpp>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSTab::DCSTab(KneeboardState* kbs) {
  mGameEventToken = AddEventListener(
    kbs->evGameEvent, [this](const GameEvent& ev) { this->OnGameEvent(ev); });
}

DCSTab::~DCSTab() {
  this->RemoveEventListener(mGameEventToken);
}

void DCSTab::OnGameEvent(const GameEvent& event) {
  if (event.name == DCS::EVT_INSTALL_PATH) {
    mInstallPath = std::filesystem::canonical(event.value);
  }

  if (event.name == DCS::EVT_SAVED_GAMES_PATH) {
    mSavedGamesPath = std::filesystem::canonical(event.value);
  }

  if (!(mInstallPath.empty() || mSavedGamesPath.empty())) {
    OnGameEvent(event, mInstallPath, mSavedGamesPath);
  }
}

std::filesystem::path DCSTab::ToAbsolutePath(
  const std::filesystem::path& maybeRelative) {
  if (maybeRelative.empty()) {
    return {};
  }

  if (std::filesystem::exists(maybeRelative)) {
    return std::filesystem::canonical(maybeRelative);
  }

  for (const auto& prefix: {mInstallPath, mSavedGamesPath}) {
    if (prefix.empty()) {
      continue;
    }

    const auto path = prefix / maybeRelative;
    if (std::filesystem::exists(path)) {
      return std::filesystem::canonical(path);
    }
  }

  return maybeRelative;
}

}// namespace OpenKneeboard
