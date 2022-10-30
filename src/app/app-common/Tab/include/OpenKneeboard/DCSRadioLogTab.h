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

#include <OpenKneeboard/PageSourceWithDelegates.h>
#include <shims/winrt/base.h>

#include <string>

#include "DCSTab.h"
#include "ITabWithSettings.h"
#include "TabBase.h"

namespace OpenKneeboard {

class PlainTextPageSource;

class DCSRadioLogTab final : public TabBase,
                             public DCSTab,
                             public PageSourceWithDelegates,
                             public ITabWithSettings {
 public:
  // Indices must match TabSettingsPage.xaml
  enum class MissionStartBehavior : uint8_t {
    DrawHorizontalLine = 0,
    ClearHistory = 1,
  };

  DCSRadioLogTab(
    const DXResources&,
    KneeboardState*,
    const std::string& title = {},
    const nlohmann::json& config = {});
  virtual ~DCSRadioLogTab();
  virtual utf8_string GetGlyph() const override;
  virtual utf8_string GetTitle() const override;
  virtual PageIndex GetPageCount() const override;
  virtual void Reload() override;

  virtual nlohmann::json GetSettings() const override;

  MissionStartBehavior GetMissionStartBehavior() const;
  void SetMissionStartBehavior(MissionStartBehavior);

  bool GetTimestampsEnabled() const;
  void SetTimestampsEnabled(bool);

 protected:
  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath) override;

 private:
  struct Settings;
  winrt::apartment_context mUIThread;
  std::shared_ptr<PlainTextPageSource> mPageSource;
  MissionStartBehavior mMissionStartBehavior {
    MissionStartBehavior::DrawHorizontalLine};
  bool mShowTimestamps = false;

  winrt::fire_and_forget OnGameEventImpl(
    const GameEvent&,
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath);

  void LoadSettings(const nlohmann::json&);
};

}// namespace OpenKneeboard
