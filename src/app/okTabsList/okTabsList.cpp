#include "okTabsList.h"

#include <OpenKneeboard/DCSAircraftTab.h>
#include <OpenKneeboard/DCSMissionTab.h>
#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/DCSTerrainTab.h>
#include <OpenKneeboard/dprint.h>

#include "TabTypes.h"
#include "okEvents.h"
#include "okTabsList_SettingsUI.h"
#include "okTabsList_SharedState.h"

using namespace OpenKneeboard;

struct okTabsList::State final : public okTabsList::SharedState {};

okTabsList::okTabsList(const nlohmann::json& config)
  : p(std::make_shared<State>()) {
  if (config.is_null()) {
    LoadDefaultConfig();
  } else {
    LoadConfig(config);
  }
}

okTabsList::~okTabsList() {
}

void okTabsList::LoadConfig(const nlohmann::json& config) {
  std::vector<nlohmann::json> tabs = config;

  for (const auto& tab: tabs) {
    if (!(tab.contains("Type") && tab.contains("Title"))) {
      continue;
    }

    const std::string title = tab.at("Title");
    const std::string type = tab.at("Type");

#define IT(_, it) \
  if (type == #it) { \
    p->tabs.push_back(std::make_shared<it##Tab>()); \
    continue; \
  }
    ZERO_CONFIG_TAB_TYPES
#undef IT

    if (!tab.contains("Settings")) {
      continue;
    }

#define IT(_, it) \
  if (type == #it) { \
    auto instance = it##Tab::FromSettings(title, tab.at("Settings")); \
    if (instance) { \
      p->tabs.push_back(instance); \
      continue; \
    } \
  }
    CONFIGURABLE_TAB_TYPES
#undef IT

    dprintf("Don't know how to load tab '{}' of type {}", title, type);
  }
}

void okTabsList::LoadDefaultConfig() {
  p->tabs = {
    std::make_shared<DCSRadioLogTab>(),
    std::make_shared<DCSMissionTab>(),
    std::make_shared<DCSAircraftTab>(),
    std::make_shared<DCSTerrainTab>(),
  };
}

std::vector<std::shared_ptr<Tab>> okTabsList::GetTabs() const {
  return p->tabs;
}

wxWindow* okTabsList::GetSettingsUI(wxWindow* parent) {
  auto ret = new SettingsUI(parent, p);
  ret->Bind(
    okEVT_SETTINGS_CHANGED, [=](auto& ev) { wxQueueEvent(this, ev.Clone()); });
  return ret;
}

nlohmann::json okTabsList::GetSettings() const {
  std::vector<nlohmann::json> ret;

  for (const auto& tab: p->tabs) {
    std::string type;
#define IT(_, it) \
  if (type.empty() && std::dynamic_pointer_cast<it##Tab>(tab)) { \
    type = #it; \
  }
    TAB_TYPES
#undef IT
    if (type.empty()) {
      dprintf("Unknown type for tab {}", tab->GetTitle());
      continue;
    }

    nlohmann::json savedTab {
      {"Type", type},
      {"Title", tab->GetTitle()},
    };

#define IT(_, it) \
  if (type == #it) { \
    ret.push_back(savedTab); \
    continue; \
  }
    ZERO_CONFIG_TAB_TYPES
#undef IT

    auto settings = tab->GetSettings();
    if (!settings.is_null()) {
      savedTab.emplace("Settings", settings);
      ret.push_back(savedTab);
      continue;
    }

    dprintf(
      "Ignoring tab '{}' of type {} as don't know how to save it.",
      tab->GetTitle(),
      type);
  }

  return ret;
}
