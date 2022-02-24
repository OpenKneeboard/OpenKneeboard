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
#include <OpenKneeboard/DCSAircraftTab.h>
#include <OpenKneeboard/DCSMissionTab.h>
#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/DCSTerrainTab.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/dprint.h>

#include <nlohmann/json.hpp>

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabState.h>
#include <OpenKneeboard/TabTypes.h>

namespace OpenKneeboard {

TabsList::TabsList(
  const DXResources& dxr,
  KneeboardState* kneeboard,
  const nlohmann::json& config)
  : mDXR(dxr), mKneeboard(kneeboard) {
  if (config.is_null()) {
    LoadDefaultConfig();
  } else {
    LoadConfig(config);
  }
}

TabsList::~TabsList() {
}

void TabsList::LoadConfig(const nlohmann::json& config) {
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
    auto instance = load_tab<it##Tab>(mDXR, title, settings); \
    if (instance) { \
      mKneeboard->AppendTab(std::make_shared<TabState>(instance)); \
      continue; \
    } \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
  }
}

void TabsList::LoadDefaultConfig() {
  mKneeboard->SetTabs({
    TabState::make_shared<DCSRadioLogTab>(mDXR),
    TabState::make_shared<DCSMissionTab>(mDXR),
    TabState::make_shared<DCSAircraftTab>(mDXR),
    TabState::make_shared<DCSTerrainTab>(mDXR),
  });
}

nlohmann::json TabsList::GetSettings() const {
  std::vector<nlohmann::json> ret;

  for (const auto& tabState: mKneeboard->GetTabs()) {
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

    auto withSettings = std::dynamic_pointer_cast<TabWithSettings>(tab);
    if (withSettings) {
      auto settings = withSettings->GetSettings();
      if (!settings.is_null()) {
        savedTab.emplace("Settings", settings);
      }
    }
    ret.push_back(savedTab);
    continue;
  }

  return ret;
}

}// namespace OpenKneeboard
