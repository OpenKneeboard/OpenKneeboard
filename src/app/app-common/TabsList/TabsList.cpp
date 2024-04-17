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
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/TabTypes.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabsList.h>

#include <OpenKneeboard/dprint.h>

#include <nlohmann/json.hpp>

#include <algorithm>

namespace OpenKneeboard {

TabsList::TabsList(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const nlohmann::json& config)
  : mDXR(dxr), mKneeboard(kneeboard) {
  LoadSettings(config);
}

TabsList::~TabsList() {
  this->RemoveAllEventListeners();
}

static std::tuple<std::string, nlohmann::json> MigrateTab(
  const std::string& type,
  const nlohmann::json& settings) {
  if (type == "PDF" || type == "TextFile") {
    return {"SingleFile", settings};
  }

  return {type, settings};
}

void TabsList::LoadSettings(const nlohmann::json& config) {
  if (config.is_null()) {
    LoadDefaultSettings();
    return;
  }
  std::vector<nlohmann::json> jsonTabs = config;

  decltype(mTabs) tabs;
  for (const auto& tab: jsonTabs) {
    if (!(tab.contains("Type") && tab.contains("Title"))) {
      continue;
    }

    const std::string title = tab.at("Title");
    const std::string rawType = tab.at("Type");

    nlohmann::json rawSettings;
    if (tab.contains("Settings")) {
      rawSettings = tab.at("Settings");
    }

    const auto [type, settings] = MigrateTab(rawType, rawSettings);

    winrt::guid persistentID {};
    if (tab.contains("ID")) {
      persistentID = winrt::guid {tab.at("ID").get<std::string>()};
      // else handled by TabBase
    }

#define IT(_, it) \
  if (type == #it) { \
    auto instance \
      = load_tab<it##Tab>(mDXR, mKneeboard, persistentID, title, settings); \
    if (instance) { \
      tabs.push_back(instance); \
      continue; \
    } \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
    dprintf("Couldn't load tab with type {}", rawType);
    OPENKNEEBOARD_BREAK;
  }
  this->SetTabs(tabs);
}

void TabsList::LoadDefaultSettings() {
  auto kbs = mKneeboard;
  this->SetTabs({
    std::make_shared<SingleFileTab>(
      mDXR,
      mKneeboard,
      Filesystem::GetRuntimeDirectory() / RuntimeFiles::QUICK_START_PDF),
    DCSRadioLogTab::Create(mDXR, mKneeboard),
    std::make_shared<DCSBriefingTab>(mDXR, mKneeboard),
    std::make_shared<DCSMissionTab>(mDXR, mKneeboard),
    std::make_shared<DCSAircraftTab>(mDXR, mKneeboard),
    std::make_shared<DCSTerrainTab>(mDXR, mKneeboard),
  });
}

nlohmann::json TabsList::GetSettings() const {
  std::vector<nlohmann::json> ret;

  for (const auto& tab: mTabs) {
    std::string type;
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
      {"ID", winrt::to_string(winrt::to_hstring(tab->GetPersistentID()))},
    };

    auto withSettings = std::dynamic_pointer_cast<ITabWithSettings>(tab);
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

std::vector<std::shared_ptr<ITab>> TabsList::GetTabs() const {
  return mTabs;
}

void TabsList::SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs) {
  if (std::ranges::equal(tabs, mTabs)) {
    return;
  }

  mTabs = tabs;
  for (auto token: mTabEvents) {
    RemoveEventListener(token);
  }
  mTabEvents.clear();
  for (const auto& tab: mTabs) {
    mTabEvents.push_back(AddEventListener(
      tab->evSettingsChangedEvent, this->evSettingsChangedEvent));
  }

  evTabsChangedEvent.Emit(mTabs);
  evSettingsChangedEvent.Emit();
}

void TabsList::InsertTab(TabIndex index, const std::shared_ptr<ITab>& tab) {
  auto tabs = mTabs;
  tabs.insert(tabs.begin() + index, tab);
  this->SetTabs(tabs);
}

void TabsList::RemoveTab(TabIndex index) {
  if (index >= mTabs.size()) {
    return;
  }

  auto tabs = mTabs;
  tabs.erase(tabs.begin() + index);
  this->SetTabs(tabs);
}

}// namespace OpenKneeboard
