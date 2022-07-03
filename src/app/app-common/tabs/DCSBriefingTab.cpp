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
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PlainTextPageSource.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

#include <chrono>
#include <format>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <LuaBridge/LuaBridge.h>

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
  const auto imageCount = mImagePages->GetPageCount();
  if (pageIndex < imageCount) {
    return mImagePages->GetNativeContentSize(pageIndex);
  }
  return mTextPages->GetNativeContentSize(pageIndex - imageCount);
}

static std::string GetCountries(const luabridge::LuaRef& countries) {
  std::string ret;
  for (auto&& [i, country]: luabridge::pairs(countries)) {
    if (
      country["static"].isNil() || country["helicopter"].isNil()
      || country["vehicle"].isNil() || country["plane"].isNil()) {
      continue;
    }
    if (!ret.empty()) {
      ret += ", ";
    }
    ret += country["name"].cast<std::string>();
  }
  return ret;
}

void DCSBriefingTab::Reload() noexcept {
  const scope_guard emitEvents([this]() {
    this->ClearContentCache();
    this->evFullyReplacedEvent.Emit();
    this->evAvailableFeaturesChangedEvent.Emit();
    this->evNeedsRepaintEvent.Emit();
  });
  mImagePages->SetPaths({});
  mTextPages->ClearText();

  if (!mMission) {
    return;
  }

  lua_State* lua = lua_open();
  scope_guard closeLua([&lua]() { lua_close(lua); });

  luaL_openlibs(lua);

  const auto root = mMission->GetExtractedPath();

  int error = luaL_dofile(lua, to_utf8(root / "mission").c_str());
  if (error) {
    dprintf("Failed to load lua mission table: {}", lua_tostring(lua, -1));
    return;
  }

  const auto localized = root / "l10n" / "DEFAULT";
  error = luaL_dofile(lua, to_utf8(localized / "dictionary").c_str());
  if (error) {
    dprintf("Failed to load lua dictionary table: {}", lua_tostring(lua, -1));
    return;
  }

  const auto mission = luabridge::getGlobal(lua, "mission");
  const auto dictionary = luabridge::getGlobal(lua, "dictionary");

  if (std::filesystem::exists(localized / "mapResource")) {
    error = luaL_dofile(lua, to_utf8(localized / "MapResource").c_str());
    if (error) {
      dprintf("Failed to load lua mapResource: {}", lua_tostring(lua, -1));
      return;
    }

    const auto mapResource = luabridge::getGlobal(lua, "mapResource");

    std::vector<std::filesystem::path> images;

    luabridge::LuaRef force = mission[CoalitionKey(
      "pictureFileNameN", "pictureFileNameR", "pictureFileNameB")];
    for (auto&& [i, resourceName]: luabridge::pairs(force)) {
      const auto fileName = mapResource[resourceName].cast<std::string>();
      const auto path = localized / fileName;
      if (std::filesystem::is_regular_file(path)) {
        images.push_back(path);
      }
    }
    mImagePages->SetPaths(images);
  }

  const std::string title = dictionary[mission["sortie"]];

  const auto startDate = mission["date"];
  const auto startSecondsSinceMidnight = mission["start_time"];
  const auto startDateTime = std::format(
    "{:04d}-{:02d}-{:02d} {:%T}",
    startDate["Year"].cast<unsigned int>(),
    startDate["Month"].cast<unsigned int>(),
    startDate["Day"].cast<unsigned int>(),
    std::chrono::seconds {
      startSecondsSinceMidnight.cast<unsigned int>(),
    });
  const std::string redCountries
    = GetCountries(mission["coalition"]["red"]["country"]);
  const std::string blueCountries
    = GetCountries(mission["coalition"]["blue"]["country"]);
  std::string_view alliedCountries;
  std::string_view enemyCountries;
  switch (mSelfData.mCoalition) {
    case DCSWorld::Coalition::Neutral:
      break;
    case DCSWorld::Coalition::Blue:
      alliedCountries = blueCountries;
      enemyCountries = redCountries;
      break;
    case DCSWorld::Coalition::Red:
      alliedCountries = redCountries;
      enemyCountries = blueCountries;
  }

  const auto situation
    = dictionary[mission["descriptionText"]].cast<std::string>();

  const auto objective = dictionary[mission[CoalitionKey(
                                      "descriptionNeutralTask",
                                      "descriptionRedTask",
                                      "descriptionBlueTask")]]
                           .cast<std::string>();

  const auto weather = mission["weather"];
  const auto temperature = weather["season"]["temperature"].cast<int>();
  const auto qnhMmHg = weather["qnh"].cast<int>();
  const auto qnhInHg = qnhMmHg / 25.4;
  const auto cloudBase = weather["clouds"]["base"].cast<int>();
  const auto wind = weather["wind"];

  mTextPages->SetText(std::format(
    _("MISSION OVERVIEW\n"
      "\n"
      "Title:    {}\n"
      "Start at: {}\n"
      "My side:  {}\n"
      "Enemies:  {}\n"
      "\n"
      "SITUATION\n"
      "\n"
      "{}\n"
      "\n"
      "OBJECTIVE\n"
      "\n"
      "{}\n"
      "\n"
      "WEATHER\n"
      "\n"
      "Temperature: {:+d}°\n"
      "QNH:         {} / {:.02f}\n"
      "Cloud cover: Base {}\n"
      "Nav wind:    At GRND {} m/s, {}° Meteo {}°\n"
      "             At 2000m {} m/s, {}° Meteo {}°\n"
      "             At 8000m {} m/s, {}° Meteo {}°"),
    title,
    startDateTime,
    alliedCountries,
    enemyCountries,
    situation,
    objective,
    temperature,
    qnhMmHg,
    qnhInHg,
    cloudBase,
    wind["atGround"]["speed"].cast<int>(),
    wind["atGround"]["dir"].cast<int>(),
    (180 + wind["atGround"]["dir"].cast<int>()) % 360,
    wind["at2000"]["speed"].cast<int>(),
    wind["at2000"]["dir"].cast<int>(),
    (180 + wind["at2000"]["dir"].cast<int>()) % 360,
    wind["at8000"]["speed"].cast<int>(),
    wind["at8000"]["dir"].cast<int>(),
    (180 + wind["at8000"]["dir"].cast<int>()) % 360));
}

void DCSBriefingTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path&,
  const std::filesystem::path&) {
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

  if (event.name == DCS::EVT_SELF_DATA) {
    auto raw = nlohmann::json::parse(event.value);
    const SelfData self {
      .mCoalition = raw.at("CoalitionID"),
      .mCountry = raw.at("Country"),
      .mAircraft = raw.at("Name"),
    };
    if (self != mSelfData) {
      mSelfData = self;
      this->Reload();
    }
  }
}

void DCSBriefingTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  const auto imageCount = mImagePages->GetPageCount();
  if (pageIndex < imageCount) {
    mImagePages->RenderPage(ctx, pageIndex, rect);
    return;
  }
  return mTextPages->RenderPage(ctx, pageIndex - imageCount, rect);
}

bool DCSBriefingTab::IsNavigationAvailable() const {
  return this->GetPageCount() > 2;
}

std::shared_ptr<ITab> DCSBriefingTab::CreateNavigationTab(
  uint16_t currentPage) {
  std::vector<NavigationTab::Entry> entries;

  const auto paths = mImagePages->GetPaths();

  for (uint16_t i = 0; i < paths.size(); ++i) {
    entries.push_back({paths.at(i).stem(), i});
  }

  const auto textCount = mTextPages->GetPageCount();
  for (uint16_t i = 0; i < textCount; ++i) {
    entries.push_back({
      std::format(_("Transcription {}/{}"), i + 1, textCount),
      static_cast<uint16_t>(i + paths.size()),
    });
  }

  return std::make_shared<NavigationTab>(
    mDXR, this, entries, this->GetNativeContentSize(currentPage));
}

}// namespace OpenKneeboard
