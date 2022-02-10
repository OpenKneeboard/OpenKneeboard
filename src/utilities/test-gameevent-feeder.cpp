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
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/Games/DCSWorld.h>
#include <Windows.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <chrono>

using namespace OpenKneeboard;
using DCS = OpenKneeboard::Games::DCSWorld;

int main() {
  printf("Feeding GameEvents to OpenKneeboard - hit Ctrl-C to exit.\n");
  ConsoleLoopCondition cliLoop;
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
    (GameEvent {
       .name = DCS::EVT_RADIO_MESSAGE,
       .value = fmt::format(
         "{}: Test message from PID {}",
         std::chrono::system_clock::now(),
         GetCurrentProcessId()),
     })
      .Send();
  } while (cliLoop.Sleep(std::chrono::seconds(1)));
  printf("Exit requested, cleaning up.\n");
  return 0;
}
