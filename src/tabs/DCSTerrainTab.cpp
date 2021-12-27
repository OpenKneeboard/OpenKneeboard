#include "OpenKneeboard/DCSTerrainTab.h"

#include <filesystem>

#include "OpenKneeboard/dprint.h"
#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab()
  : DCSTab(_("Local")) {
}

const char* DCSTerrainTab::GetGameEventName() const {
  return DCS::EVT_TERRAIN;
}

void DCSTerrainTab::Update(const std::filesystem::path& installPath, const std::string& value) {
  auto path = installPath / "Mods" / "terrains" / value / "Kneeboard";
  this->SetPath(path);
}

}// namespace OpenKneeboard
