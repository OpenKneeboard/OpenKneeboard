#include "OpenKneeboard/DCSTerrainTab.h"

#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/GameEvent.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab() : FolderTab(_("Map"), {}) {
}

void DCSTerrainTab::OnGameEvent(const GameEvent& event) {
  if (event.Name == DCS::EVT_TERRAIN) {
    LoadTerrain(event.Value);
  }
}

void DCSTerrainTab::LoadTerrain(const std::string& terrain) {
  if (terrain == mTerrain) {
    return;
  }
}

}// namespace OpenKneeboard
