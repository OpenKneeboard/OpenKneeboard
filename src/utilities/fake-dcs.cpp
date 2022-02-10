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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/Games/DCSWorld.h>
#include <OpenKneeboard/utf8.h>
#include <Windows.h>
#include <fmt/format.h>

using DCS = OpenKneeboard::Games::DCSWorld;
using namespace OpenKneeboard;

int main() {
  const auto savedGamePath = DCS::GetSavedGamesPath(DCS::Version::OPEN_BETA);
  const auto installPath = DCS::GetInstalledPath(DCS::Version::OPEN_BETA);

  printf(
    "DCS: %S\nSaved Games: %S\n", installPath.c_str(), savedGamePath.c_str());

  (GameEvent {DCS::EVT_INSTALL_PATH, to_utf8(installPath)}).Send();
  (GameEvent {DCS::EVT_SAVED_GAMES_PATH, to_utf8(savedGamePath)}).Send();
  (GameEvent {DCS::EVT_AIRCRAFT, "A-10C_2"}).Send();
  (GameEvent {DCS::EVT_TERRAIN, "Caucasus"}).Send();

  const std::vector<const char*> messages = {
    "Simple single line",
    "Word wrap012345678901234567890123456789012345678901234567890"
    "--34567890123456789012345678901234567890123456789012345678901234567890"
    "--34567890123456789012345678901234567890123456789012345678901234567890",
    "One Break\nMore",
    "normal line",
    "Two Break\n\nMore",
    "normal line",
    "Trailing break\n",
    "normal line",
  };

  for (int i = 0; i < 4; ++i) {
    for (auto message: messages) {
      (GameEvent {DCS::EVT_RADIO_MESSAGE, message}).Send();
    }
  }

  printf("Sent data.\n");

  return 0;
}
