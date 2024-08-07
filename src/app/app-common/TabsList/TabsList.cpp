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
#include <OpenKneeboard/DCSAircraftTab.hpp>
#include <OpenKneeboard/DCSMissionTab.hpp>
#include <OpenKneeboard/DCSRadioLogTab.hpp>
#include <OpenKneeboard/DCSTerrainTab.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/PluginTab.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/TabTypes.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabsList.hpp>

#include <shims/nlohmann/json.hpp>

#include <OpenKneeboard/dprint.hpp>

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
    if (type == "Plugin") {
      auto instance = std::make_shared<PluginTab>(
        mDXR, mKneeboard, persistentID, title, settings);
      tabs.push_back(instance);
      continue;
    }
    dprintf("Couldn't load tab with type {}", rawType);
    OPENKNEEBOARD_BREAK;
  }
  fire_and_forget(this->SetTabs(tabs));
}

void TabsList::LoadDefaultSettings() {
  fire_and_forget(this->SetTabs({
    std::make_shared<SingleFileTab>(
      mDXR,
      mKneeboard,
      Filesystem::GetRuntimeDirectory() / RuntimeFiles::QUICK_START_PDF),
    DCSRadioLogTab::Create(mDXR, mKneeboard),
    std::make_shared<DCSBriefingTab>(mDXR, mKneeboard),
    std::make_shared<DCSMissionTab>(mDXR, mKneeboard),
    std::make_shared<DCSAircraftTab>(mDXR, mKneeboard),
    std::make_shared<DCSTerrainTab>(mDXR, mKneeboard),
  }));
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
    if (type.empty() && std::dynamic_pointer_cast<PluginTab>(tab)) {
      type = "Plugin";
    }
    if (type.empty()) {
      dprintf("Unknown type for tab {}", tab->GetTitle());
      OPENKNEEBOARD_BREAK;
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

IAsyncAction TabsList::SetTabs(std::vector<std::shared_ptr<ITab>> tabs) {
  if (std::ranges::equal(tabs, mTabs)) {
    co_return;
  }

  std::vector<IAsyncAction> disposers;
  for (auto tab: mTabs) {
    if (!std::ranges::contains(tabs, tab->GetRuntimeID(), [](auto it) {
          return it->GetRuntimeID();
        })) {
      if (auto p = std::dynamic_pointer_cast<IHasDisposeAsync>(tab)) {
        disposers.push_back(p->DisposeAsync());
      }
    }
  }
  for (auto& it: disposers) {
    co_await it;
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

IAsyncAction TabsList::InsertTab(TabIndex index, std::shared_ptr<ITab> tab) {
  auto tabs = mTabs;
  tabs.insert(tabs.begin() + index, tab);
  co_await this->SetTabs(tabs);
}

IAsyncAction TabsList::RemoveTab(TabIndex index) {
  if (index >= mTabs.size()) {
    co_return;
  }

  auto tabs = mTabs;
  tabs.erase(tabs.begin() + index);
  co_await this->SetTabs(tabs);
}

}// namespace OpenKneeboard
