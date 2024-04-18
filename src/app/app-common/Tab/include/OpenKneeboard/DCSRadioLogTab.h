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
#include "ITabWithSettings.h"
#include "TabBase.h"

#include <OpenKneeboard/PageSourceWithDelegates.h>

#include <OpenKneeboard/audited_ptr.h>

#include <shims/winrt/base.h>

#include <string>

namespace OpenKneeboard {

class PlainTextPageSource;

class DCSRadioLogTab final
  : public TabBase,
    public DCSTab,
    public PageSourceWithDelegates,
    public ITabWithSettings,
    public std::enable_shared_from_this<DCSRadioLogTab> {
 public:
  // Indices must match TabsSettingsPage.xaml
  enum class MissionStartBehavior : uint8_t {
    DrawHorizontalLine = 0,
    ClearHistory = 1,
  };

  static std::shared_ptr<DCSRadioLogTab> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*);
  static std::shared_ptr<DCSRadioLogTab> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json& config);
  virtual ~DCSRadioLogTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual PageIndex GetPageCount() const override;
  virtual void Reload() override;

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

  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath) override;

 private:
  winrt::apartment_context mUIThread;
  std::shared_ptr<PlainTextPageSource> mPageSource;
  MissionStartBehavior mMissionStartBehavior {
    MissionStartBehavior::DrawHorizontalLine};
  bool mShowTimestamps = false;

  winrt::fire_and_forget OnGameEventImpl(
    GameEvent,
    std::filesystem::path installPath,
    std::filesystem::path savedGamesPath);

  void LoadSettings(const nlohmann::json&);
};

}// namespace OpenKneeboard
