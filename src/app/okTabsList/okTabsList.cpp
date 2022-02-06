/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include "okTabsList.h"

#include <OpenKneeboard/DCSAircraftTab.h>
#include <OpenKneeboard/DCSMissionTab.h>
#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/DCSTerrainTab.h>
#include <OpenKneeboard/dprint.h>

#include "KneeboardState.h"
#include "TabState.h"
#include "TabTypes.h"
#include "okEvents.h"
#include "okTabsList_SettingsUI.h"
#include "okTabsList_SharedState.h"

using namespace OpenKneeboard;

struct okTabsList::State final : public okTabsList::SharedState {};

okTabsList::okTabsList(
  const DXResources& dxr,
  const std::shared_ptr<KneeboardState>& kneeboard,
  const nlohmann::json& config)
  : p(std::make_shared<State>()) {
  p->mDXR = dxr;
  p->mKneeboard = kneeboard;

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

    nlohmann::json settings;
    if (tab.contains("Settings")) {
      settings = tab.at("Settings");
    }

#define IT(_, it) \
  if (type == #it) { \
    auto instance = load_tab<it##Tab>(p->mDXR, title, settings); \
    if (instance) { \
      p->mKneeboard->AppendTab(std::make_shared<TabState>(instance)); \
      continue; \
    } \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
  }
}

void okTabsList::LoadDefaultConfig() {
  p->mKneeboard->SetTabs({
    TabState::make_shared<DCSRadioLogTab>(p->mDXR),
    TabState::make_shared<DCSMissionTab>(p->mDXR),
    TabState::make_shared<DCSAircraftTab>(p->mDXR),
    TabState::make_shared<DCSTerrainTab>(p->mDXR),
  });
}

wxWindow* okTabsList::GetSettingsUI(wxWindow* parent) {
  auto ret = new SettingsUI(parent, p);
  ret->Bind(
    okEVT_SETTINGS_CHANGED, [=](auto& ev) { wxQueueEvent(this, ev.Clone()); });
  return ret;
}

nlohmann::json okTabsList::GetSettings() const {
  std::vector<nlohmann::json> ret;

  for (const auto& tabState: p->mKneeboard->GetTabs()) {
    std::string type;
    auto tab = tabState->GetRootTab();
#define IT(_, it) \
  if (type.empty() && std::dynamic_pointer_cast<it##Tab>(tab)) { \
    type = #it; \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
    if (type.empty()) {
      dprintf("Unknown type for tab {}", tab->GetTitle());
      continue;
    }

    nlohmann::json savedTab {
      {"Type", type},
      {"Title", tab->GetTitle()},
    };

    auto settings = tab->GetSettings();
    if (!settings.is_null()) {
      savedTab.emplace("Settings", settings);
    }
    ret.push_back(savedTab);
    continue;
  }

  return ret;
}
