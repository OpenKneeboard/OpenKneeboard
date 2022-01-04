#include "OpenKneeboard/DCSTab.h"

#include <filesystem>

#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

class DCSTab::Impl final {
 public:
  struct Config final {
    std::filesystem::path InstallPath;
    std::filesystem::path SavedGamesPath;
    std::string Value;
    bool operator==(const Config&) const = default;
  };

  Config CurrentConfig;
  Config LastValidConfig;

  std::string LastValue;
};

DCSTab::DCSTab(const wxString& title)
  : Tab(title), p(std::make_shared<Impl>()) {
}

DCSTab::~DCSTab() {
}

void DCSTab::OnGameEvent(const GameEvent& event) {
  if (event.Name == this->GetGameEventName()) {
    p->CurrentConfig.Value = event.Value;
    Update();
    return;
  }

  if (event.Name == DCS::EVT_INSTALL_PATH) {
    p->CurrentConfig.InstallPath = std::filesystem::canonical(event.Value);
    Update();
    return;
  }

  if (event.Name == DCS::EVT_SAVED_GAMES_PATH) {
    p->CurrentConfig.SavedGamesPath = std::filesystem::canonical(event.Value);
    Update();
    return;
  }

  if (event.Name == DCS::EVT_SIMULATION_START) {
    this->OnSimulationStart();
    return;
  }
}

void DCSTab::Update() {
  auto c = p->CurrentConfig;
  if (c == p->LastValidConfig) {
    return;
  }

  if (c.InstallPath.empty()) {
    return;
  }

  if (c.SavedGamesPath.empty()) {
    return;
  }

  if (c.Value == p->LastValue) {
    return;
  }
  p->LastValue = c.Value;

  this->Update(c.InstallPath, c.SavedGamesPath, c.Value);
}

void DCSTab::OnSimulationStart() {
}

}// namespace OpenKneeboard
