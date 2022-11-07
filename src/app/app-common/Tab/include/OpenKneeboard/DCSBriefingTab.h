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

#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>

#include "DCSTab.h"
#include "TabBase.h"

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
  DCSBriefingTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID = {});
  virtual ~DCSBriefingTab();
  virtual utf8_string GetGlyph() const override;
  virtual utf8_string GetTitle() const override;

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
