#include "OpenKneeboard/DCSTerrainTab.h"

#include <filesystem>

#include "OpenKneeboard/FolderTab.h"
#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab()
  : DCSTab(_("Theater")), mDelegate(std::make_shared<FolderTab>("", "")) {
}

DCSTerrainTab::~DCSTerrainTab() {
}

void DCSTerrainTab::Reload() {
  mDelegate->Reload();
}

uint16_t DCSTerrainTab::GetPageCount() const {
  return mDelegate->GetPageCount();
}
wxImage DCSTerrainTab::RenderPage(uint16_t index) {
  return mDelegate->RenderPage(index);
}

const char* DCSTerrainTab::GetGameEventName() const {
  return DCS::EVT_TERRAIN;
}

void DCSTerrainTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& _savedGamesPath,
  const std::string& value) {
  auto path = installPath / "Mods" / "terrains" / value / "Kneeboard";
  mDelegate->SetPath(path);
}

}// namespace OpenKneeboard
