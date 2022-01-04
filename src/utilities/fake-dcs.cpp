#include <ShlObj.h>
#include <Windows.h>

#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"

#include <fmt/format.h>

using DCS = OpenKneeboard::Games::DCSWorld;
using namespace OpenKneeboard;

int main() {
  const char AIRCRAFT[] = "A-10C_2";
  const char TERRAIN[] = "Caucasus";

  std::filesystem::path savedGamePath;
  {
    wchar_t* buffer = nullptr;
    if (
      SHGetKnownFolderPath(FOLDERID_SavedGames, NULL, NULL, &buffer) == S_OK
      && buffer) {
      savedGamePath
        = std::filesystem::canonical(std::wstring(buffer)) / "DCS.openbeta";
    }
  }
  const auto installPath = DCS::GetOpenBetaPath();

  printf(
    "DCS: %S\nSaved Games: %S\n", installPath.c_str(), savedGamePath.c_str());

  (GameEvent {DCS::EVT_INSTALL_PATH, installPath.string()}).Send();
  (GameEvent {DCS::EVT_SAVED_GAMES_PATH, savedGamePath.string()}).Send();
  (GameEvent {DCS::EVT_AIRCRAFT, AIRCRAFT}).Send();
  (GameEvent {DCS::EVT_TERRAIN, TERRAIN}).Send();

  for (auto i = 0; i < 15; ++i) {
    auto message = fmt::format("{}a - single line", i);
    (GameEvent { DCS::EVT_RADIO_MESSAGE, message }).Send();
    message = fmt::format(
      "{}b - multi line - 12345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890", i);
    (GameEvent { DCS::EVT_RADIO_MESSAGE, message }).Send();
  }

  printf("Sent data.\n");

  return 0;
}
