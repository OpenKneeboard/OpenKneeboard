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
#include <OpenKneeboard/DCSMagneticModel.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/ImagePageSource.h>
#include <OpenKneeboard/Lua.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PlainTextPageSource.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

#include <GeographicLib/DMS.hpp>
#include <GeographicLib/GeoCoords.hpp>
#include <GeographicLib/TransverseMercator.hpp>
#include <GeographicLib/UTMUPS.hpp>
#include <chrono>
#include <cmath>
#include <format>
#include <limits>

using DCS = OpenKneeboard::DCSWorld;

static_assert(std::is_same_v<GeographicLib::Math::real, DCS::GeoReal>);

namespace OpenKneeboard {

class DCSGrid final {
 public:
  DCSGrid(DCSWorld::GeoReal originLat, DCSWorld::GeoReal originLong);
  std::tuple<DCSWorld::GeoReal, DCSWorld::GeoReal> LatLongFromXY(
    DCSWorld::GeoReal x,
    DCSWorld::GeoReal y) const;

 private:
  DCSWorld::GeoReal mOffsetX;
  DCSWorld::GeoReal mOffsetY;
  DCSWorld::GeoReal mZoneMeridian;

  static GeographicLib::TransverseMercator sModel;
};

GeographicLib::TransverseMercator DCSGrid::sModel
  = GeographicLib::TransverseMercator::UTM();

DCSGrid::DCSGrid(DCSWorld::GeoReal originLat, DCSWorld::GeoReal originLong) {
  const int zone = GeographicLib::UTMUPS::StandardZone(originLat, originLong);
  mZoneMeridian = (6.0 * zone - 183);

  sModel.Forward(mZoneMeridian, originLat, originLong, mOffsetX, mOffsetY);

  dprintf(
    "DCS (0, 0) is in UTM zone {}, with meridian at {} and a UTM offset of "
    "({}, {})",
    zone,
    mZoneMeridian,
    mOffsetX,
    mOffsetY);
}

std::tuple<DCSWorld::GeoReal, DCSWorld::GeoReal> DCSGrid::LatLongFromXY(
  DCSWorld::GeoReal dcsX,
  DCSWorld::GeoReal dcsY) const {
  // UTM (x, y) are (easting, northing), but DCS (x, y) are (northing, easting)
  const auto x = mOffsetX + dcsY;
  const auto y = mOffsetY + dcsX;
  DCSWorld::GeoReal retLat, retLong;

  sModel.Reverse(mZoneMeridian, x, y, retLat, retLong);
  return {retLat, retLong};
}

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

static std::string GetCountries(const LuaRef& countries) {
  std::string ret;
  for (auto&& [i, country]: countries) {
    if (!(country.contains("static") && country.contains("helicopter")
          && country.contains("vehicle") && country.contains("plane"))) {
      continue;
    }
    if (!ret.empty()) {
      ret += ", ";
    }
    ret += country["name"].Get<std::string>();
  }
  return ret;
}

struct DCSBriefingWind {
  DCSBriefingWind(const LuaRef& data) {
    mSpeed = data["speed"].Cast<float>();
    mDirection = data["dir"].Cast<int>();
    mStandardDirection = (180 + mDirection) % 360;
    if (mDirection == 0) {
      mDirection = 360;
    }
    if (mStandardDirection == 0) {
      mStandardDirection = 360;
    }
    mSpeedInKnots = mSpeed * 1.94384f;
  }

  float mSpeed;
  float mSpeedInKnots;
  int mDirection;
  int mStandardDirection;
};

static std::string DMSFormat(DCSWorld::GeoReal angle, char pos, char neg) {
  DCSWorld::GeoReal degrees, minutes, seconds;
  GeographicLib::DMS::Encode(angle, degrees, minutes, seconds);
  return std::format(
    "{} {:03.0f}°{:02.0f}'{:05.2f}\"",
    degrees >= 0 ? pos : neg,
    std::abs(degrees),
    minutes,
    seconds);
};

static std::string DMFormat(DCSWorld::GeoReal angle, char pos, char neg) {
  DCSWorld::GeoReal degrees, minutes;
  GeographicLib::DMS::Encode(angle, degrees, minutes);
  return std::format(
    "{} {:03.0f}°{:05.3f}'",
    degrees >= 0 ? pos : neg,
    std::abs(degrees),
    minutes);
}

static std::string MGRSFormat(double latitude, double longitude) {
  const auto raw
    = GeographicLib::GeoCoords(latitude, longitude).MGRSRepresentation(0);
  // e.g. 37TEHnnnnneeeee
  //                ^ -5
  //           ^ -10
  //         ^-12
  const std::string_view view(raw);
  return std::format(
    "{} {} {} {}",
    view.substr(0, view.size() - 12),
    view.substr(view.size() - 12, 2),
    view.substr(view.size() - 10, 5),
    view.substr(view.size() - 5, 5));
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

std::shared_ptr<ITab> DCSBriefingTab::CreateNavigationTab(
  uint16_t currentPage) {
  std::vector<NavigationTab::Entry> entries;

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

  return std::make_shared<NavigationTab>(
    mDXR, this, entries, this->GetNativeContentSize(currentPage));
}

void DCSBriefingTab::SetMissionImages(
  const LuaRef& mission,
  const LuaRef& mapResource,
  const std::filesystem::path& resourcePath) try {
  std::vector<std::filesystem::path> images;

  const auto force = mission.at(
    CoalitionKey("pictureFileNameN", "pictureFileNameR", "pictureFileNameB"));
  for (auto&& [i, resourceName]: force) {
    const auto fileName = mapResource[resourceName].Cast<std::string>();
    const auto path = resourcePath / fileName;
    if (std::filesystem::is_regular_file(path)) {
      images.push_back(path);
    }
  }
  mImagePages->SetPaths(images);
} catch (const LuaIndexError& e) {
  dprintf("LuaIndexError when loading images: {}", e.what());
}

void DCSBriefingTab::PushMissionOverview(
  const LuaRef& mission,
  const LuaRef& dictionary) try {
  const std::string title = dictionary[mission["sortie"]];

  const auto startDate = mission["date"];
  const auto startSecondsSinceMidnight = mission["start_time"];
  const auto startDateTime = std::format(
    "{:04d}-{:02d}-{:02d} {:%T}",
    startDate["Year"].Cast<unsigned int>(),
    startDate["Month"].Cast<unsigned int>(),
    startDate["Day"].Cast<unsigned int>(),
    std::chrono::seconds {
      startSecondsSinceMidnight.Cast<unsigned int>(),
    });

  std::string redCountries = _("Unknown.");
  try {
    redCountries = GetCountries(mission["coalition"]["red"]["country"]);
  } catch (const LuaIndexError&) {
  }

  std::string blueCountries = _("Unknown.");
  try {
    blueCountries = GetCountries(mission["coalition"]["blue"]["country"]);
  } catch (const LuaIndexError&) {
  }

  std::string_view alliedCountries;
  std::string_view enemyCountries;
  switch (mDCSState.mCoalition) {
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

  mTextPages->PushMessage(std::format(
    _("MISSION OVERVIEW\n"
      "\n"
      "Title:    {}\n"
      "Start at: {}\n"
      "My side:  {}\n"
      "Enemies:  {}"),
    title,
    startDateTime,
    alliedCountries,
    enemyCountries));
} catch (const LuaIndexError& e) {
  dprintf("LuaIndexError when loading mission overview: {}", e.what());
}

void DCSBriefingTab::PushMissionWeather(const LuaRef& mission) try {
  const auto weather = mission["weather"];
  const auto temperature = weather["season"]["temperature"].Cast<int>();
  const auto qnhMmHg = weather["qnh"].Cast<int>();
  const auto qnhInHg = qnhMmHg / 25.4;
  const auto cloudBase = weather["clouds"]["base"].Cast<int>();
  const auto wind = weather["wind"];
  DCSBriefingWind windAtGround {wind["atGround"]};
  DCSBriefingWind windAt2000 {wind["at2000"]};
  DCSBriefingWind windAt8000 {wind["at8000"]};

  mTextPages->PushMessage(std::format(
    _("WEATHER\n"
      "\n"
      "Temperature: {:+d}°\n"
      "QNH:         {} / {:.02f}\n"
      "Cloud cover: Base {}\n"
      "Nav wind:    At GRND {:.0f} m/s, {}° Meteo {}°\n"
      "             At 2000m {:.0f} m/s, {}° Meteo {}°\n"
      "             At 8000m {:.0f} m/s, {}° Meteo {}°"),
    temperature,
    qnhMmHg,
    qnhInHg,
    cloudBase,
    windAtGround.mSpeed,
    windAtGround.mDirection,
    windAtGround.mStandardDirection,
    windAt2000.mSpeed,
    windAt2000.mDirection,
    windAt2000.mStandardDirection,
    windAt8000.mSpeed,
    windAt8000.mDirection,
    windAt8000.mStandardDirection));

} catch (const LuaIndexError& e) {
  dprintf("LuaIndexError when loading mission weather: {}", e.what());
}

void DCSBriefingTab::PushBullseyeData(const LuaRef& mission) try {
  if (!mDCSState.mOrigin) {
    return;
  }
  if (mDCSState.mCoalition == DCS::Coalition::Neutral) {
    return;
  }

  double magVar = 0.0f;

  const auto& origin = mDCSState.mOrigin;
  DCSGrid grid(origin->mLat, origin->mLong);

  const auto key = CoalitionKey("neutral", "red", "blue");
  const auto startDate = mission.at("date");
  const auto xyBulls = mission["coalition"][key]["bullseye"];
  const auto [bullsLat, bullsLong] = grid.LatLongFromXY(
    xyBulls["x"].Cast<DCSWorld::GeoReal>(),
    xyBulls["y"].Cast<DCSWorld::GeoReal>());

  DCSMagneticModel magModel(mInstallationPath);
  magVar = magModel.GetMagneticVariation(
    std::chrono::year_month_day {
      std::chrono::year {startDate["Year"].Cast<int>()},
      std::chrono::month {startDate["Month"].Cast<unsigned>()},
      std::chrono::day {startDate["Day"].Cast<unsigned>()},
    },
    static_cast<float>(bullsLat),
    static_cast<float>(bullsLong));

  mTextPages->PushMessage(std::format(
    _("BULLSEYE\n"
      "\n"
      "Position: {} {}\n"
      "          {}   {}\n"
      "          {:08.4f}, {:08.4f}\n"
      "          {}\n"
      "MagVar:   {:.01f}°"),
    DMSFormat(bullsLat, 'N', 'S'),
    DMSFormat(bullsLong, 'E', 'W'),
    DMFormat(bullsLat, 'N', 'S'),
    DMFormat(bullsLong, 'E', 'W'),
    bullsLat,
    bullsLong,
    MGRSFormat(bullsLat, bullsLong),
    magVar));

  if (!mDCSState.mAircraft.starts_with("A-10C")) {
    return;
  }

  const auto weather = mission["weather"];
  const auto wind = mission["wind"];
  const auto temperature = weather["season"]["temperature"].Cast<int>();
  DCSBriefingWind windAtGround {wind["atGround"]};
  DCSBriefingWind windAt2000 {wind["at2000"]};
  DCSBriefingWind windAt8000 {wind["at8000"]};

  mTextPages->PushMessage(std::format(
    _("A-10C LASTE WIND\n"
      "\n"
      "Using bullseye magvar: {:.1f}°\n"
      "\n"
      "ALT WIND   TEMP\n"
      "00  {:03.0f}/{:02.0f} {}\n"
      "01  {:03.0f}/{:02.0f} {}\n"
      "02  {:03.0f}/{:02.0f} {}\n"
      "07  {:03.0f}/{:02.0f} {}\n"
      "26  {:03.0f}/{:02.0f} {}"),
    magVar,
    // 0ft/ground
    windAtGround.mStandardDirection - magVar,
    windAtGround.mSpeedInKnots,
    temperature,
    // 1000ft
    windAtGround.mStandardDirection - magVar,
    windAtGround.mSpeedInKnots * 2,
    temperature - 2,
    // 2000ft
    windAtGround.mStandardDirection - magVar,
    windAtGround.mSpeedInKnots * 2,
    temperature - (2 * 2),
    // 7000ft/2000m
    windAt2000.mStandardDirection - magVar,
    windAt2000.mSpeedInKnots,
    temperature - (2 * 7),
    // 26000ft/8000m
    windAt8000.mStandardDirection - magVar,
    windAt8000.mSpeedInKnots,
    temperature - (2 * 26)));
} catch (const LuaIndexError& e) {
  dprintf("LuaIndexError when loading mission bullseye data: {}", e.what());
}

void DCSBriefingTab::PushMissionSituation(
  const LuaRef& mission,
  const LuaRef& dictionary) try {
  mTextPages->PushMessage(std::format(
    _("SITUATION\n"
      "\n"
      "{}"),
    dictionary[mission["descriptionText"]].Get<std::string>()));
} catch (const LuaIndexError& e) {
  dprintf("LuaIndexError when loading mission situation: {}", e.what());
}

void DCSBriefingTab::PushMissionObjective(
  const LuaRef& mission,
  const LuaRef& dictionary) try {
  mTextPages->PushMessage(std::format(
    _("OBJECTIVE\n"
      "\n"
      "{}"),
    dictionary[mission[CoalitionKey(
                 "descriptionNeutralTask",
                 "descriptionRedTask",
                 "descriptionBlueTask")]]
      .Get<std::string>()));
} catch (const LuaIndexError& e) {
  dprintf("LuaIndexError when loading mission objective: {}", e.what());
}

}// namespace OpenKneeboard
