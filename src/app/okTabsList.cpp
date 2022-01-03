#include "okTabsList.h"

#include "OpenKneeboard/DCSAircraftTab.h"
#include "OpenKneeboard/DCSMissionTab.h"
#include "OpenKneeboard/DCSRadioLogTab.h"
#include "OpenKneeboard/DCSTerrainTab.h"

using namespace OpenKneeboard;

okTabsList::okTabsList(const nlohmann::json& config) {
  mTabs = {
    std::make_shared<DCSRadioLogTab>(),
    std::make_shared<DCSMissionTab>(),
    std::make_shared<DCSAircraftTab>(),
    std::make_shared<DCSTerrainTab>(),
  };
}

okTabsList::~okTabsList() {
}

std::vector<std::shared_ptr<Tab>> okTabsList::GetTabs() const {
  return mTabs;
}

wxWindow* okTabsList::GetSettingsUI(wxWindow* parent) {
  return nullptr;
}

nlohmann::json okTabsList::GetSettings() const {
  return {};
}
