// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "DCSTab.hpp"
#include "ITabWithSettings.hpp"
#include "TabBase.hpp"

#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <string>

namespace OpenKneeboard {

class PlainTextPageSource;

class DCSRadioLogTab final : public TabBase,
                             public DCSTab,
                             public PageSourceWithDelegates,
                             public ITabWithSettings {
 public:
  // Indices must match TabsSettingsPage.xaml
  enum class MissionStartBehavior : uint8_t {
    DrawHorizontalLine = 0,
    ClearHistory = 1,
  };

  static task<std::shared_ptr<DCSRadioLogTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*);
  static task<std::shared_ptr<DCSRadioLogTab>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    winrt::guid persistentID,
    std::string_view title,
    nlohmann::json config);
  virtual ~DCSRadioLogTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual PageIndex GetPageCount() const override;
  [[nodiscard]]
  virtual task<void> Reload() override;

  virtual nlohmann::json GetSettings() const override;

  MissionStartBehavior GetMissionStartBehavior() const;
  void SetMissionStartBehavior(MissionStartBehavior);

  bool GetTimestampsEnabled() const;
  void SetTimestampsEnabled(bool);

 protected:
  explicit DCSRadioLogTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json& config);

  virtual OpenKneeboard::fire_and_forget OnAPIEvent(
    APIEvent,
    std::filesystem::path installPath,
    std::filesystem::path savedGamesPath) override;

 private:
  winrt::apartment_context mUIThread;
  std::shared_ptr<PlainTextPageSource> mPageSource;
  MissionStartBehavior mMissionStartBehavior {
    MissionStartBehavior::DrawHorizontalLine};
  bool mShowTimestamps = false;

  void LoadSettings(const nlohmann::json&);
};

}// namespace OpenKneeboard
