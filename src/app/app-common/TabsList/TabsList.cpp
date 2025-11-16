// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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

#include <OpenKneeboard/dprint.hpp>

#include <shims/nlohmann/json.hpp>

#include <algorithm>

namespace OpenKneeboard {

TabsList::TabsList(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard)
  : mDXR(dxr), mKneeboard(kneeboard) {
}

task<std::shared_ptr<TabsList>> TabsList::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const nlohmann::json& config) {
  std::unique_ptr<TabsList> ret {new TabsList(dxr, kneeboard)};
  co_await ret->LoadSettings(config);
  co_return ret;
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

task<std::shared_ptr<ITab>> TabsList::LoadTabFromJSON(
  const nlohmann::json tab) {
  if (!(tab.contains("Type") && tab.contains("Title"))) {
    co_return nullptr;
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

  try {
#define IT(_, it) \
  if (type == #it) { \
    auto instance = co_await load_tab<it##Tab>( \
      mDXR, mKneeboard, persistentID, title, settings); \
    if (instance) { \
      co_return instance; \
    } \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
    if (type == "Plugin") {
      auto instance = co_await PluginTab::Create(
        mDXR, mKneeboard, persistentID, title, settings);
      co_return instance;
    }
  } catch (const std::exception& e) {
    dprint.Error(
      "Failed to load tab {} with std::exception: {}",
      tab.value<std::string>("ID", "<no GUID>"),
      e.what());
    throw;
  } catch (const winrt::hresult_error& e) {
    dprint.Error(
      "Failed to load tab {} with HRESULT: {}",
      tab.value<std::string>("ID", "<no GUID>"),
      winrt::to_string(e.message()));
    throw;
  }
  dprint("Couldn't load tab with type {}", rawType);
  OPENKNEEBOARD_BREAK;
  co_return nullptr;
}

task<void> TabsList::LoadSettings(nlohmann::json config) {
  if (config.is_null()) {
    co_await LoadDefaultSettings();
    co_return;
  }
  const std::vector<nlohmann::json> jsonTabs = config;

  std::vector<task<std::shared_ptr<ITab>>> awaitables;
  for (auto&& tab: jsonTabs) {
    awaitables.push_back(this->LoadTabFromJSON(tab));
  }

  decltype(mTabs) tabs;
  for (auto&& it: awaitables) {
    auto tab = co_await std::move(it);
    if (tab) {
      tabs.push_back(std::move(tab));
    }
  }

  co_await this->SetTabs(tabs);
}

task<void> TabsList::LoadDefaultSettings() {
  // This is a little awkwardly structured because MSVC 2022 17.11 crashes if I
  // put the `co_await`s in the SetTabs initializer list
  auto quickStartPending = SingleFileTab::Create(
    mDXR,
    mKneeboard,
    Filesystem::GetRuntimeDirectory() / RuntimeFiles::QUICK_START_PDF);
  auto radioLogPending = DCSRadioLogTab::Create(mDXR, mKneeboard);
  auto briefingPending = DCSBriefingTab::Create(mDXR, mKneeboard);

  auto quickStart = co_await std::move(quickStartPending);
  auto radioLog = co_await std::move(radioLogPending);
  auto briefing = co_await std::move(briefingPending);

  co_await this->SetTabs({
    quickStart,
    radioLog,
    briefing,
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
    if (type.empty() && std::dynamic_pointer_cast<PluginTab>(tab)) {
      type = "Plugin";
    }
    if (type.empty()) {
      dprint("Unknown type for tab {}", tab->GetTitle());
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

task<void> TabsList::SetTabs(std::vector<std::shared_ptr<ITab>> tabs) {
  if (std::ranges::equal(tabs, mTabs)) {
    co_return;
  }

  {
    const auto oldTabs = std::exchange(mTabs, {});
    std::vector<task<void>> disposers;
    for (auto tab: oldTabs) {
      if (!std::ranges::contains(tabs, tab->GetRuntimeID(), [](auto it) {
            return it->GetRuntimeID();
          })) {
        if (auto p = std::dynamic_pointer_cast<IHasDisposeAsync>(tab)) {
          disposers.push_back(p->DisposeAsync());
        }
      }
    }
    for (auto& it: disposers) {
      co_await std::move(it);
    }
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

  evTabsChangedEvent.Emit();
  evSettingsChangedEvent.Emit();
}

task<void> TabsList::InsertTab(TabIndex index, std::shared_ptr<ITab> tab) {
  auto tabs = mTabs;
  tabs.insert(tabs.begin() + index, tab);
  co_await this->SetTabs(tabs);
}

task<void> TabsList::RemoveTab(TabIndex index) {
  if (index >= mTabs.size()) {
    co_return;
  }

  auto tabs = mTabs;
  tabs.erase(tabs.begin() + index);
  co_await this->SetTabs(tabs);
}

}// namespace OpenKneeboard
