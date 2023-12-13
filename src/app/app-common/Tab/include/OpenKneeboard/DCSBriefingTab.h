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
#pragma once

#include "DCSTab.h"
#include "TabBase.h"

#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>

namespace OpenKneeboard {

class LuaRef;
class DCSExtractedMission;
class ImageFilePageSource;
class PlainTextPageSource;
class KneeboardState;
struct DXResources;

class DCSBriefingTab final : public TabBase,
                             public virtual DCSTab,
                             public virtual PageSourceWithDelegates {
 public:
  DCSBriefingTab(const DXResources&, KneeboardState*);
  DCSBriefingTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  virtual ~DCSBriefingTab();
  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual void Reload() noexcept override;

  bool IsNavigationAvailable() const override;
  std::vector<NavigationEntry> GetNavigationEntries() const override;

 protected:
  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path&,
    const std::filesystem::path&) override;

 private:
  std::shared_ptr<DCSExtractedMission> mMission;
  std::shared_ptr<ImageFilePageSource> mImagePages;
  std::shared_ptr<PlainTextPageSource> mTextPages;
  std::filesystem::path mInstallationPath;

  struct LatLong {
    DCSWorld::GeoReal mLat;
    DCSWorld::GeoReal mLong;

    auto operator<=>(const LatLong&) const noexcept = default;
  };

  struct DCSState {
    DCSWorld::Coalition mCoalition {DCSWorld::Coalition::Neutral};
    int mCountry = -1;
    std::string mAircraft;
    std::optional<LatLong> mOrigin;

    auto operator<=>(const DCSState&) const = default;
  };
  DCSState mDCSState;
  KneeboardState* mKneeboard;

  constexpr const char* CoalitionKey(
    const char* neutralKey,
    const char* redforKey,
    const char* blueforKey) {
    switch (mDCSState.mCoalition) {
      case DCSWorld::Coalition::Neutral:
        return neutralKey;
      case DCSWorld::Coalition::Red:
        return redforKey;
      case DCSWorld::Coalition::Blue:
        return blueforKey;
    }
    throw std::logic_error("Invalid coalition");
  }

  /** DCS supports localised text being stored in a dictionary.
   *
   * In this case the string in mission Lua will start with DictKey_ and should
   * be used as a key to reference in the dictionary. If it doesn't start with
   * DictKey_ it can be used directly. This has only been observed in older
   * files - DCS mission editor by default seems to put everything in the
   * dictionary now.
   *
   * @param mission mission lua object
   * @param dictionary dictionary lua object
   * @param key key to lookup
   */
  std::string GetMissionText(
    const LuaRef& mission,
    const LuaRef& dictionary,
    const char* key);

  void SetMissionImages(
    const LuaRef& mission,
    const LuaRef& mapResource,
    const std::filesystem::path& localizedResourcePath);

  void PushMissionOverview(const LuaRef& mission, const LuaRef& dictionary);
  void PushMissionSituation(const LuaRef& mission, const LuaRef& dictionary);
  void PushMissionObjective(const LuaRef& mission, const LuaRef& dictionary);
  void PushMissionWeather(const LuaRef& mission);
  void PushBullseyeData(const LuaRef& mission);
};

}// namespace OpenKneeboard
