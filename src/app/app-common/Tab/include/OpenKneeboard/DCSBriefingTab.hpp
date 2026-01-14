// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "DCSTab.hpp"
#include "TabBase.hpp"

#include <OpenKneeboard/DCSEvents.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

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
  static task<std::shared_ptr<DCSBriefingTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*);
  static task<std::shared_ptr<DCSBriefingTab>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    winrt::guid persistentID,
    std::string_view title);
  virtual ~DCSBriefingTab();
  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  [[nodiscard]]
  virtual task<void> Reload() noexcept override;

  bool IsNavigationAvailable() const override;
  std::vector<NavigationEntry> GetNavigationEntries() const override;

 protected:
  virtual OpenKneeboard::fire_and_forget
    OnAPIEvent(APIEvent, std::filesystem::path, std::filesystem::path) override;

 private:
  DCSBriefingTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  KneeboardState* mKneeboard {nullptr};

  std::shared_ptr<DCSExtractedMission> mMission;
  std::shared_ptr<ImageFilePageSource> mImagePages;
  std::shared_ptr<PlainTextPageSource> mTextPages;
  std::filesystem::path mInstallationPath;

  struct LatLong {
    DCSEvents::GeoReal mLat;
    DCSEvents::GeoReal mLong;

    auto operator<=>(const LatLong&) const noexcept = default;
  };

  struct DCSState {
    DCSEvents::Coalition mCoalition {DCSEvents::Coalition::Neutral};
    int mCountry = -1;
    std::string mAircraft;
    std::optional<LatLong> mOrigin;

    auto operator<=>(const DCSState&) const = default;
  };
  DCSState mDCSState;

  inline const char* CoalitionKey(
    const char* neutralKey,
    const char* redforKey,
    const char* blueforKey) {
    switch (mDCSState.mCoalition) {
      case DCSEvents::Coalition::Neutral:
        return neutralKey;
      case DCSEvents::Coalition::Red:
        return redforKey;
      case DCSEvents::Coalition::Blue:
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
