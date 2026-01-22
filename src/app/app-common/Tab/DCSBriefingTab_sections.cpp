// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/Coordinates.hpp>
#include <OpenKneeboard/DCSBriefingTab.hpp>
#include <OpenKneeboard/DCSEvents.hpp>
#include <OpenKneeboard/DCSExtractedMission.hpp>
#include <OpenKneeboard/DCSGrid.hpp>
#include <OpenKneeboard/DCSMagneticModel.hpp>
#include <OpenKneeboard/ImageFilePageSource.hpp>
#include <OpenKneeboard/Lua.hpp>
#include <OpenKneeboard/PlainTextPageSource.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <chrono>
#include <cmath>
#include <format>

static_assert(
  std::same_as<OpenKneeboard::DCSEvents::GeoReal, OpenKneeboard::GeoReal>);

using namespace OpenKneeboard::Coordinates;

namespace OpenKneeboard {

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
    mSpeed = data["speed"];
    mDirection = data["dir"];
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

void DCSBriefingTab::SetMissionImages(
  const LuaRef& mission,
  const LuaRef& mapResource,
  const std::filesystem::path& resourcePath) try {
  std::vector<std::filesystem::path> images;

  const auto force = mission.at(
    CoalitionKey("pictureFileNameN", "pictureFileNameR", "pictureFileNameB"));
  for (auto&& [i, resourceName]: force) {
    // `resourceName` will almost always be a string starting with `ResKey_`,
    // which will be an entry in the `mapResource` dictionary created in
    // `l10n\DEFAULT\mapResource`; this isn't required, and some missions
    // just containing a raw filename instead.
    const auto fileName = mapResource.contains(resourceName)
      ? mapResource[resourceName].Get<std::string>()
      : resourceName.Get<std::string>();
    const auto path = resourcePath / fileName;
    if (std::filesystem::is_regular_file(path)) {
      images.push_back(path);
    }
  }
  mImagePages->SetPaths(images);
} catch (const LuaIndexError& e) {
  dprint("LuaIndexError when loading images: {}", e.what());
}

void DCSBriefingTab::PushMissionOverview(
  const LuaRef& mission,
  const LuaRef& dictionary) try {
  const std::string title = GetMissionText(mission, dictionary, "sortie");

  const auto startDate = mission["date"];
  const auto startSecondsSinceMidnight = mission["start_time"];
  const auto startDateTime = std::format(
    "{:04d}-{:02d}-{:02d} {:%T}",
    startDate["Year"].Get<uint16_t>(),
    startDate["Month"].Get<uint16_t>(),
    startDate["Day"].Get<uint16_t>(),
    std::chrono::seconds {
      startSecondsSinceMidnight.Get<unsigned int>(),
    });

  std::string redCountries = _("Unknown.");
  try {
    redCountries = GetCountries(mission["coalition"]["red"]["country"]);
  } catch (const LuaIndexError&) {}

  std::string blueCountries = _("Unknown.");
  try {
    blueCountries = GetCountries(mission["coalition"]["blue"]["country"]);
  } catch (const LuaIndexError&) {}

  std::string_view alliedCountries;
  std::string_view enemyCountries;
  switch (mDCSState.mCoalition) {
    case DCSEvents::Coalition::Neutral:
      break;
    case DCSEvents::Coalition::Blue:
      alliedCountries = blueCountries;
      enemyCountries = redCountries;
      break;
    case DCSEvents::Coalition::Red:
      alliedCountries = redCountries;
      enemyCountries = blueCountries;
  }

  mTextPages->PushMessage(
    std::format(
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
  dprint("LuaIndexError when loading mission overview: {}", e.what());
}

void DCSBriefingTab::PushMissionWeather(const LuaRef& mission) try {
  const auto weather = mission["weather"];
  const auto temperature = weather["season"]["temperature"].Get<int>();
  const auto qnhMmHg = weather["qnh"].Get<float>();
  const auto qnhInHg = qnhMmHg / 25.4;
  const auto cloudBase = weather["clouds"]["base"].Get<int>();
  const auto wind = weather["wind"];
  DCSBriefingWind windAtGround {wind["atGround"]};
  DCSBriefingWind windAt2000 {wind["at2000"]};
  DCSBriefingWind windAt8000 {wind["at8000"]};

  mTextPages->PushMessage(
    std::format(
      _("WEATHER\n"
        "\n"
        "Temperature: {:+d}°\n"
        "QNH:         {} / {:.02f}\n"
        "Cloud cover: Base {}\n"
        "Nav wind:    At GRND {:.0f} m/s, {}° Meteo {}°\n"
        "             At 2000m {:.0f} m/s, {}° Meteo {}°\n"
        "             At 8000m {:.0f} m/s, {}° Meteo {}°"),
      temperature,
      // Match DCS: round down, not to nearest
      static_cast<int>(qnhMmHg),
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
  dprint("LuaIndexError when loading mission weather: {}", e.what());
}

void DCSBriefingTab::PushBullseyeData(const LuaRef& mission) try {
  if (!mDCSState.mOrigin) {
    return;
  }
  if (mDCSState.mCoalition == DCSEvents::Coalition::Neutral) {
    return;
  }

  double magVar = 0.0f;

  const auto& origin = mDCSState.mOrigin;
  DCSGrid grid(origin->mLat, origin->mLong);

  const auto key = CoalitionKey("neutral", "red", "blue");
  const auto startDate = mission.at("date");
  const auto xyBulls = mission["coalition"][key]["bullseye"];
  const auto [bullsLat, bullsLong] = grid.LatLongFromXY(
    xyBulls["x"].Get<DCSEvents::GeoReal>(),
    xyBulls["y"].Get<DCSEvents::GeoReal>());

  DCSMagneticModel magModel(mInstallationPath);
  magVar = magModel.GetMagneticVariation(
    std::chrono::year_month_day {
      std::chrono::year {startDate["Year"].Get<int>()},
      std::chrono::month {startDate["Month"].Get<unsigned>()},
      std::chrono::day {startDate["Day"].Get<unsigned>()},
    },
    static_cast<float>(bullsLat),
    static_cast<float>(bullsLong));

  mTextPages->PushMessage(
    std::format(
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
  const auto wind = weather["wind"];
  const auto temperature = weather["season"]["temperature"].Get<int>();
  DCSBriefingWind windAtGround {wind["atGround"]};
  DCSBriefingWind windAt2000 {wind["at2000"]};
  DCSBriefingWind windAt8000 {wind["at8000"]};

  mTextPages->PushMessage(
    std::format(
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
  dprint("LuaIndexError when loading mission bullseye data: {}", e.what());
}

std::string DCSBriefingTab::GetMissionText(
  const LuaRef& mission,
  const LuaRef& dictionary,
  const char* key) {
  auto mission_value = mission[key].Get<std::string>();
  if (mission_value.starts_with("DictKey_")) {
    return dictionary[mission[key]].Get<std::string>();
  } else {
    return mission_value;
  }
}

void DCSBriefingTab::PushMissionSituation(
  const LuaRef& mission,
  const LuaRef& dictionary) try {
  mTextPages->PushMessage(
    std::format(
      _("SITUATION\n"
        "\n"
        "{}"),
      GetMissionText(mission, dictionary, "descriptionText")));
} catch (const LuaIndexError& e) {
  dprint("LuaIndexError when loading mission situation: {}", e.what());
}

void DCSBriefingTab::PushMissionObjective(
  const LuaRef& mission,
  const LuaRef& dictionary) try {
  mTextPages->PushMessage(
    std::format(
      _("OBJECTIVE\n"
        "\n"
        "{}"),
      GetMissionText(
        mission,
        dictionary,
        CoalitionKey(
          "descriptionNeutralTask",
          "descriptionRedTask",
          "descriptionBlueTask"))));
} catch (const LuaIndexError& e) {
  dprint("LuaIndexError when loading mission objective: {}", e.what());
}

}// namespace OpenKneeboard
