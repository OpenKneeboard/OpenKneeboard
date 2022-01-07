#include "OpenKneeboard/DCSTerrainTab.h"

#include <filesystem>

#include "OpenKneeboard/FolderTab.h"
#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"
#include "okEvents.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSTerrainTab::DCSTerrainTab()
  : DCSTab(_("Theater")), mDelegate(std::make_shared<FolderTab>("", "")) {
  mDelegate->Bind(okEVT_TAB_FULLY_REPLACED, [this](auto& ev) {
    wxQueueEvent(this, ev.Clone());
  });
}

DCSTerrainTab::~DCSTerrainTab() {
}

void DCSTerrainTab::Reload() {
  mDelegate->Reload();
}

uint16_t DCSTerrainTab::GetPageCount() const {
  return mDelegate->GetPageCount();
}

void DCSTerrainTab::RenderPage(
  uint16_t pageIndex,
  const winrt::com_ptr<ID2D1RenderTarget>& target,
  const D2D1_RECT_F& rect) {
  mDelegate->RenderPage(pageIndex, target, rect);
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
