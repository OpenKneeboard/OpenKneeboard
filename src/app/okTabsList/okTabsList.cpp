#include "okTabsList.h"

#include "OpenKneeboard/DCSAircraftTab.h"
#include "OpenKneeboard/DCSMissionTab.h"
#include "OpenKneeboard/DCSRadioLogTab.h"
#include "OpenKneeboard/DCSTerrainTab.h"
#include "okTabsList_SettingsUI.h"
#include "okTabsList_SharedState.h"
#include "okEvents.h"

using namespace OpenKneeboard;

struct okTabsList::State final : public okTabsList::SharedState {};

okTabsList::okTabsList(const nlohmann::json& config)
  : p(std::make_shared<State>()) {
  p->Tabs = {
    std::make_shared<DCSRadioLogTab>(),
    std::make_shared<DCSMissionTab>(),
    std::make_shared<DCSAircraftTab>(),
    std::make_shared<DCSTerrainTab>(),
  };
}

okTabsList::~okTabsList() {
}

std::vector<std::shared_ptr<Tab>> okTabsList::GetTabs() const {
  return p->Tabs;
}

wxWindow* okTabsList::GetSettingsUI(wxWindow* parent) {
  auto ret = new SettingsUI(parent, p);
  ret->Bind(okEVT_SETTINGS_CHANGED, [=](auto& ev) {
    wxQueueEvent(this, ev.Clone());
  });
  return ret;
}

nlohmann::json okTabsList::GetSettings() const {
  return {};
}
