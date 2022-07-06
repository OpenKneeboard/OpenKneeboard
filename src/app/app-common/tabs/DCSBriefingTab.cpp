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

#include <GeographicLib/DMS.hpp>
#include <GeographicLib/GeoCoords.hpp>
#include <GeographicLib/TransverseMercator.hpp>
#include <GeographicLib/UTMUPS.hpp>
#include <chrono>
#include <cmath>
#include <format>
#include <limits>

extern "C" {
#include <GeomagnetismHeader.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <LuaBridge/LuaBridge.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

class DCSMagneticModel {
 public:
  DCSMagneticModel(const std::filesystem::path& dcsInstallation);
  ~DCSMagneticModel();

  float GetMagneticVariation(
    const std::chrono::year_month_day& date,
    float latitude,
    float longitude) const;

 private:
  MAGtype_MagneticModel* GetModel(
    const std::chrono::year_month_day& date) const;
  std::vector<MAGtype_MagneticModel*> mModels;
};

DCSMagneticModel::DCSMagneticModel(
  const std::filesystem::path& dcsInstallation) {
  const auto cofDir = dcsInstallation / "Data" / "MagVar" / "COF";

  for (const auto& file: std::filesystem::directory_iterator(cofDir)) {
    if (!std::filesystem::is_regular_file(file)) {
      continue;
    }
    MAGtype_MagneticModel* model = nullptr;
    MAG_robustReadMagModels(
      const_cast<char*>(file.path().string().c_str()),
      reinterpret_cast<MAGtype_MagneticModel*(*)[]>(&model),
      1);
    mModels.push_back(model);
  }
}

DCSMagneticModel::~DCSMagneticModel() {
  for (auto model: mModels) {
    MAG_FreeMagneticModelMemory(model);
  }
}

MAGtype_MagneticModel* DCSMagneticModel::GetModel(
  const std::chrono::year_month_day& date) const {
  MAGtype_Date magDate {
    static_cast<int>(date.year()),
    static_cast<int>(static_cast<unsigned>(date.month())),
    static_cast<int>(static_cast<unsigned>(date.day())),
  };
  char error[512];
  MAG_DateToYear(&magDate, error);
  const auto year = magDate.DecimalYear;

  for (auto model: mModels) {
    if (year < model->epoch) {
      dprintf(
        "No WMM model for historical year {}, using incorrect {:.0f} model",
        date.year(),
        model->epoch);
      return model;
    }

    if (year - model->epoch <= 5) {
      dprintf(
        "Using correct WMM {:0.0f} model for year {}",
        model->epoch,
        date.year());
      return model;
    }
  }

  auto model = mModels.back();

  dprintf(
    "No WMM model found for future year {}, using incorrect {:.0f} model",
    date.year(),
    model->epoch);
  return model;
}

float DCSMagneticModel::GetMagneticVariation(
  const std::chrono::year_month_day& date,
  float latitude,
  float longitude) const {
  const MAGtype_CoordGeodetic geoCoord {
    .lambda = longitude,
    .phi = latitude,
  };

  MAGtype_Ellipsoid ellipsoid;
  MAGtype_Geoid geoid;
  MAG_SetDefaults(&ellipsoid, &geoid);
  MAGtype_CoordSpherical sphereCoord {};
  MAG_GeodeticToSpherical(ellipsoid, geoCoord, &sphereCoord);

  MAGtype_MagneticModel* model = this->GetModel(date);
  MAGtype_MagneticModel* timedModel;
  const auto nMax = model->nMax;
  // Taken from wmm_point.c sample
  timedModel = MAG_AllocateModelMemory((nMax + 1) * (nMax + 2) / 2);
  scope_guard freeTimedModel(
    [=]() { MAG_FreeMagneticModelMemory(timedModel); });

  MAGtype_Date magDate {
    static_cast<int>(date.year()),
    static_cast<int>(static_cast<unsigned>(date.month())),
    static_cast<int>(static_cast<unsigned>(date.day())),
  };
  char error[512];
  MAG_DateToYear(&magDate, error);
  MAG_TimelyModifyMagneticModel(magDate, model, timedModel);

  MAGtype_GeoMagneticElements geoElements {};
  MAG_Geomag(ellipsoid, sphereCoord, geoCoord, timedModel, &geoElements);

  return geoElements.Decl;
}

class DCSGrid final {
 public:
  DCSGrid(double originLat, double originLong);
  std::tuple<double, double> LatLongFromXY(double x, double y) const;

 private:
  double mOffsetX;
  double mOffsetY;
  double mZoneMeridian;

  static GeographicLib::TransverseMercator sModel;
};

GeographicLib::TransverseMercator DCSGrid::sModel
  = GeographicLib::TransverseMercator::UTM();

DCSGrid::DCSGrid(double originLat, double originLong) {
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

std::tuple<double, double> DCSGrid::LatLongFromXY(double dcsX, double dcsY)
  const {
  // UTM (x, y) are (easting, northing), but DCS (x, y) are (northing, easting)
  const auto x = mOffsetX + dcsY;
  const auto y = mOffsetY + dcsX;
  double retLat, retLong;

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

struct DCSBriefingWind {
  DCSBriefingWind(const luabridge::LuaRef& data) {
    mSpeed = data["speed"].cast<float>();
    mDirection = data["dir"].cast<int>();
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

static std::string DMSFormat(double angle, char pos, char neg) {
  double degrees, minutes, seconds;
  GeographicLib::DMS::Encode(angle, degrees, minutes, seconds);
  return std::format(
    "{} {:03.0f}°{:02.0f}'{:05.2f}\"",
    degrees >= 0 ? pos : neg,
    std::abs(degrees),
    minutes,
    seconds);
};

static std::string DMFormat(double angle, char pos, char neg) {
  double degrees, minutes;
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

  mTextPages->PushMessage(std::format(
    _("SITUATION\n"
      "\n"
      "{}"),
    dictionary[mission["descriptionText"]].cast<std::string>()));

  mTextPages->PushMessage(std::format(
    _("OBJECTIVE\n"
      "\n"
      "{}"),
    dictionary[mission[CoalitionKey(
                 "descriptionNeutralTask",
                 "descriptionRedTask",
                 "descriptionBlueTask")]]
      .cast<std::string>()));

  double magVar = 0.0f;

  if (mDCSState.mOrigin && mDCSState.mCoalition != DCS::Coalition::Neutral) {
    const auto& origin = mDCSState.mOrigin;
    DCSGrid grid(origin->mLat, origin->mLong);

    const auto key = CoalitionKey("neutral", "red", "blue");
    const auto xyBulls = mission["coalition"][key]["bullseye"];
    const auto [bullsLat, bullsLong] = grid.LatLongFromXY(
      xyBulls["x"].cast<double>(), xyBulls["y"].cast<double>());

    DCSMagneticModel magModel(mInstallationPath);
    magVar = magModel.GetMagneticVariation(
      std::chrono::year_month_day {
        std::chrono::year {startDate["Year"].cast<int>()},
        std::chrono::month {startDate["Month"].cast<unsigned>()},
        std::chrono::day {startDate["Day"].cast<unsigned>()},
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
  }

  const auto weather = mission["weather"];
  const auto temperature = weather["season"]["temperature"].cast<int>();
  const auto qnhMmHg = weather["qnh"].cast<int>();
  const auto qnhInHg = qnhMmHg / 25.4;
  const auto cloudBase = weather["clouds"]["base"].cast<int>();
  const auto wind = weather["wind"];
  DCSBriefingWind windAtGround {luabridge::LuaRef {wind["atGround"]}};
  DCSBriefingWind windAt2000 {luabridge::LuaRef {wind["at2000"]}};
  DCSBriefingWind windAt8000 {luabridge::LuaRef {wind["at8000"]}};

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

  if (mDCSState.mAircraft.starts_with("A-10C")) {
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
  }

  this->ClearContentCache();
  this->evFullyReplacedEvent.Emit();
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

}// namespace OpenKneeboard
