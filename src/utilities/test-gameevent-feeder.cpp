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
#include <OpenKneeboard/ConsoleLoopCondition.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/GameEvent.h>
#include <Windows.h>

#include <chrono>
#include <format>

using namespace OpenKneeboard;
using DCS = OpenKneeboard::DCSWorld;

int main() {
  printf("Feeding GameEvents to OpenKneeboard - hit Ctrl-C to exit.\n");
  ConsoleLoopCondition cliLoop;
  const auto pid = GetCurrentProcessId();
  do {
    (GameEvent {
       .name = DCS::EVT_SAVED_GAMES_PATH,
       .value = DCS::GetSavedGamesPath(DCS::Version::OPEN_BETA),
     })
      .Send();
    (GameEvent {
       .name = DCS::EVT_INSTALL_PATH,
       .value = DCS::GetInstalledPath(DCS::Version::OPEN_BETA),
     })
      .Send();

    const auto now = std::chrono::system_clock::now();
    (GameEvent {
       .name = DCS::EVT_MESSAGE,
       .value
       = (nlohmann::json(DCSWorld::MessageEvent {
            .message = std::format("{}: Test message from PID {}", now, pid),
            .messageType = DCSWorld::MessageType::Radio,
            .missionTime = std::chrono::duration_cast<std::chrono::seconds>(
                             now.time_since_epoch())
                             .count(),
          }))
           .dump()})
      .Send();
  } while (cliLoop.Sleep(std::chrono::seconds(1)));
  printf("Exit requested, cleaning up.\n");
  return 0;
}
