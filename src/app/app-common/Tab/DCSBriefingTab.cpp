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

#include <OpenKneeboard/DCSBriefingTab.h>
#include <OpenKneeboard/DCSExtractedMission.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/ImagePageSource.h>
#include <OpenKneeboard/Lua.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PlainTextPageSource.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSBriefingTab::DCSBriefingTab(const DXResources& dxr, KneeboardState* kbs)
  : TabWithDoodles(dxr, kbs),
    mDXR(dxr),
    mImagePages(std::make_unique<ImagePageSource>(dxr)),
    mTextPages(std::make_unique<PlainTextPageSource>(dxr, _("[no briefing]"))) {
}

DCSBriefingTab::~DCSBriefingTab() {
  this->RemoveAllEventListeners();
}

utf8_string DCSBriefingTab::GetGlyph() const {
  return "\uE95D";
}

utf8_string DCSBriefingTab::GetTitle() const {
  return _("Briefing");
}

uint16_t DCSBriefingTab::GetPageCount() const {
  return mImagePages->GetPageCount() + mTextPages->GetPageCount();
}

D2D1_SIZE_U DCSBriefingTab::GetNativeContentSize(uint16_t pageIndex) {
  const auto textPageCount = mTextPages->GetPageCount();
  if (pageIndex < textPageCount) {
    return mTextPages->GetNativeContentSize(pageIndex);
  }
  return mImagePages->GetNativeContentSize(pageIndex - textPageCount);
}

void DCSBriefingTab::Reload() noexcept {
  const scope_guard emitEvents([this]() {
    this->ClearContentCache();
    this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
    this->evAvailableFeaturesChangedEvent.Emit();
    this->evNeedsRepaintEvent.Emit();
  });
  mImagePages->SetPaths({});
  mTextPages->ClearText();

  if (!mMission) {
    return;
  }

  const auto root = mMission->GetExtractedPath();
  if (!std::filesystem::exists(root / "mission")) {
    return;
  }

  const auto localized = root / "l10n" / "DEFAULT";

  LuaState lua;
  lua.DoFile(root / "mission");
  if (std::filesystem::exists(localized / "dictionary")) {
    lua.DoFile(localized / "dictionary");
  }
  if (std::filesystem::exists(localized / "mapResource")) {
    lua.DoFile(localized / "MapResource");
  }

  const auto mission = lua.GetGlobal("mission");
  const auto dictionary = lua.GetGlobal("dictionary");
  const auto mapResource = lua.GetGlobal("mapResource");

  this->SetMissionImages(mission, mapResource, localized);
  this->PushMissionOverview(mission, dictionary);
  this->PushMissionSituation(mission, dictionary);
  this->PushMissionObjective(mission, dictionary);
  this->PushMissionWeather(mission);
  this->PushBullseyeData(mission);

  this->ClearContentCache();
  this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

void DCSBriefingTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& installPath,
  const std::filesystem::path&) {
  mInstallationPath = installPath;
  if (event.name == DCS::EVT_MISSION) {
    const auto missionZip = std::filesystem::canonical(event.value);

    if (mMission && mMission->GetZipPath() == missionZip) {
      return;
    }

    mMission = DCSExtractedMission::Get(missionZip);
    dprintf("Briefing tab: loading {}", missionZip);
    this->Reload();
    return;
  }

  auto state = mDCSState;
  if (event.name == DCS::EVT_SELF_DATA) {
    auto raw = nlohmann::json::parse(event.value);
    state.mCoalition = raw.at("CoalitionID"),
    state.mCountry = raw.at("Country");
    state.mAircraft = raw.at("Name");
  } else if (event.name == DCS::EVT_ORIGIN) {
    auto raw = nlohmann::json::parse(event.value);
    state.mOrigin = LatLong {
      .mLat = raw["latitude"],
      .mLong = raw["longitude"],
    };
  } else {
    return;
  }

  if (state != mDCSState) {
    mDCSState = state;
    this->Reload();
  }
}

void DCSBriefingTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  const auto textPageCount = mTextPages->GetPageCount();
  if (pageIndex < textPageCount) {
    mTextPages->RenderPage(ctx, pageIndex, rect);
    return;
  }
  mImagePages->RenderPage(ctx, pageIndex - textPageCount, rect);
}

bool DCSBriefingTab::IsNavigationAvailable() const {
  return this->GetPageCount() > 2;
}

std::vector<NavigationEntry> DCSBriefingTab::GetNavigationEntries() const {
  std::vector<NavigationEntry> entries;

  const auto textCount = mTextPages->GetPageCount();
  for (uint16_t i = 0; i < textCount; ++i) {
    entries.push_back({
      std::format(_("Transcription {}/{}"), i + 1, textCount),
      i,
    });
  }

  const auto paths = mImagePages->GetPaths();

  for (uint16_t i = 0; i < paths.size(); ++i) {
    entries.push_back(
      {paths.at(i).stem(), static_cast<uint16_t>(i + textCount)});
  }

  return entries;
}

}// namespace OpenKneeboard
