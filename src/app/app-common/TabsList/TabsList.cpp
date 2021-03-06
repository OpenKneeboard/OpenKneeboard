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
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/PDFTab.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/TabTypes.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/dprint.h>

#include <nlohmann/json.hpp>

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
    auto instance = load_tab<it##Tab>(mDXR, mKneeboard, title, settings); \
    if (instance) { \
      mKneeboard->AppendTab(instance); \
      continue; \
    } \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
  }
}

void TabsList::LoadDefaultConfig() {
  wchar_t exePathStr[MAX_PATH];
  auto exePathStrLen = GetModuleFileNameW(NULL, exePathStr, MAX_PATH);
  const auto exeDir
    = std::filesystem::path(std::wstring_view {exePathStr, exePathStrLen})
        .parent_path();

  auto kbs = mKneeboard;
  mKneeboard->SetTabs({
    std::make_shared<PDFTab>(
      mDXR,
      mKneeboard,
      _("Quick Start"),
      exeDir / RuntimeFiles::QUICK_START_PDF),
    std::make_shared<DCSRadioLogTab>(mDXR, mKneeboard),
    std::make_shared<DCSBriefingTab>(mDXR, mKneeboard),
    std::make_shared<DCSMissionTab>(mDXR, mKneeboard),
    std::make_shared<DCSAircraftTab>(mDXR, mKneeboard),
    std::make_shared<DCSTerrainTab>(mDXR, mKneeboard),
  });
}

nlohmann::json TabsList::GetSettings() const {
  std::vector<nlohmann::json> ret;

  for (const auto& tab: mKneeboard->GetTabs()) {
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

}// namespace OpenKneeboard
