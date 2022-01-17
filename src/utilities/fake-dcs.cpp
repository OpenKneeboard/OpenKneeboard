#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/Games/DCSWorld.h>
#include <Windows.h>
#include <fmt/format.h>

using DCS = OpenKneeboard::Games::DCSWorld;
using namespace OpenKneeboard;

int main() {
  const auto savedGamePath = DCS::GetSavedGamesPath(DCS::Version::OPEN_BETA);
  const auto installPath = DCS::GetInstalledPath(DCS::Version::OPEN_BETA);

  printf(
    "DCS: %S\nSaved Games: %S\n", installPath.c_str(), savedGamePath.c_str());

  (GameEvent {DCS::EVT_INSTALL_PATH, installPath.string()}).Send();
  (GameEvent {DCS::EVT_SAVED_GAMES_PATH, savedGamePath.string()}).Send();
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
