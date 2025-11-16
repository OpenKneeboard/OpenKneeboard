// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/ConsoleLoopCondition.hpp>
#include <OpenKneeboard/DCSWorld.hpp>

#include <Windows.h>

#include <chrono>
#include <format>

using namespace OpenKneeboard;
using DCS = OpenKneeboard::DCSWorld;

int main() {
  printf("Feeding APIEvents to OpenKneeboard - hit Ctrl-C to exit.\n");
  ConsoleLoopCondition cliLoop;
  const auto pid = GetCurrentProcessId();
  do {
    (APIEvent {
       .name = DCS::EVT_SAVED_GAMES_PATH,
       .value = to_utf8(DCS::GetSavedGamesPath(DCS::Version::OPEN_BETA)),
     })
      .Send();
    (APIEvent {
       .name = DCS::EVT_INSTALL_PATH,
       .value = to_utf8(DCS::GetInstalledPath(DCS::Version::OPEN_BETA)),
     })
      .Send();

    const auto now = std::chrono::system_clock::now();
    (APIEvent {
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
