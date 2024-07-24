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
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/DCSWorld.hpp>

#include <Windows.h>

#include <OpenKneeboard/utf8.hpp>

#include <format>

using DCS = OpenKneeboard::DCSWorld;
using namespace OpenKneeboard;

int main() {
  const auto savedGamePath = DCS::GetSavedGamesPath(DCS::Version::OPEN_BETA);
  const auto installPath = DCS::GetInstalledPath(DCS::Version::OPEN_BETA);

  printf(
    "DCS: %S\nSaved Games: %S\n", installPath.c_str(), savedGamePath.c_str());

  (APIEvent {DCS::EVT_INSTALL_PATH, to_utf8(installPath)}).Send();
  (APIEvent {DCS::EVT_SAVED_GAMES_PATH, to_utf8(savedGamePath)}).Send();
  (APIEvent {DCS::EVT_AIRCRAFT, "A-10C_2"}).Send();
  (APIEvent {DCS::EVT_TERRAIN, "Caucasus"}).Send();
  (APIEvent {
     DCS::EVT_MISSION,
     "C:\\Program Files\\Eagle Dynamics\\DCS World "
     "OpenBeta\\Mods\\aircraft\\Ka-50\\Missions\\Campaigns\\ATO-A-P2.2.miz",
   })
    .Send();

  const std::vector<const char*> messages = {
    "Simple single line",
    "wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap"
    " wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap"
    " wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap wrap",
    "tab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\t"
    "tab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\ttab\t",
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
      auto payload = DCSWorld::MessageEvent {
        .message = message,
        .messageType = DCSWorld::MessageType::Radio,
        .missionTime = 0,
      };
      nlohmann::json j = payload;
      (APIEvent {DCS::EVT_MESSAGE, j.dump()}).Send();
    }
  }

  printf("Sent data.\n");

  return 0;
}
