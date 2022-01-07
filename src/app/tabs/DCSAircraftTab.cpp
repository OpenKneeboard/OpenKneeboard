#include "OpenKneeboard/DCSAircraftTab.h"

#include <filesystem>

#include "OpenKneeboard/FolderTab.h"
#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"
#include "okEvents.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

DCSAircraftTab::DCSAircraftTab()
  : DCSTab(_("Aircraft")), mDelegate(std::make_shared<FolderTab>("", "")) {
  mDelegate->Bind(okEVT_TAB_FULLY_REPLACED, [this](auto& ev) {
    wxQueueEvent(this, ev.Clone());
  });
}

DCSAircraftTab::~DCSAircraftTab() {
}

void DCSAircraftTab::Reload() {
  mDelegate->Reload();
}

uint16_t DCSAircraftTab::GetPageCount() const {
  return mDelegate->GetPageCount();
}

void DCSAircraftTab::RenderPage(
  uint16_t pageIndex,
  const winrt::com_ptr<ID2D1RenderTarget>& target,
  const D2D1_RECT_F& rect) {
  mDelegate->RenderPage(pageIndex, target, rect);
}

const char* DCSAircraftTab::GetGameEventName() const {
  return DCS::EVT_AIRCRAFT;
}

void DCSAircraftTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath,
  const std::string& aircraft) {
  auto path = savedGamesPath / "KNEEBOARD" / aircraft;
  mDelegate->SetPath(path);
}

}// namespace OpenKneeboard
