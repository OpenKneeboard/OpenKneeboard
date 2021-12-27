#include "OpenKneeboard/DCSTerrainTab.h"

#include <filesystem>

#include "OpenKneeboard/dprint.h"
#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

class DCSTerrainTab::Impl final {
 public:
  struct Config final {
    std::filesystem::path InstallPath;
    std::string Terrain;
    bool operator==(const Config&) const = default;
  };

  Config CurrentConfig;
  Config LastValidConfig;
};

DCSTerrainTab::DCSTerrainTab()
  : FolderTab(_("Local"), {}), p(std::make_shared<Impl>()) {
}

void DCSTerrainTab::OnGameEvent(const GameEvent& event) {
  if (event.Name == DCS::EVT_TERRAIN) {
    p->CurrentConfig.Terrain = event.Value;
    Update();
    return;
  }

  if (event.Name == DCS::EVT_INSTALL_PATH) {
    p->CurrentConfig.InstallPath = std::filesystem::canonical(event.Value);
    Update();
    return;
  }
}

void DCSTerrainTab::Update() {
  auto c = p->CurrentConfig;
  if (c == p->LastValidConfig) {
    return;
  }

  if (c.InstallPath.empty() || c.Terrain.empty()) {
    return;
  }

  auto path = c.InstallPath / "Mods" / "terrains" / c.Terrain / "Kneeboard";
  dprintf("Loading terrain from {}", path.string());

  this->SetPath(path);
}

}// namespace OpenKneeboard
