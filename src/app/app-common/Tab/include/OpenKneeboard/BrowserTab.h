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

#include <OpenKneeboard/ITabWithSettings.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>
#include <OpenKneeboard/TabBase.h>
#include <OpenKneeboard/WebView2PageSource.h>

#include <OpenKneeboard/audited_ptr.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/json.h>

namespace OpenKneeboard {

class HWNDPageSource;

class BrowserTab final : public TabBase,
                         public ITabWithSettings,
                         public PageSourceWithDelegates {
 public:
  struct Settings;
  BrowserTab() = delete;
  BrowserTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const Settings&);
  virtual ~BrowserTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual void Reload() final override;

  struct Settings : WebView2PageSource::Settings {
    constexpr bool operator==(const Settings&) const noexcept = default;
  };

  nlohmann::json GetSettings() const override;

  bool IsSimHubIntegrationEnabled() const;
  void SetSimHubIntegrationEnabled(bool);

  bool IsBackgroundTransparent() const;
  void SetBackgroundTransparent(bool);

  bool IsDeveloperToolsWindowEnabled() const;
  void SetDeveloperToolsWindowEnabled(bool);

 private:
  winrt::apartment_context mUIThread;
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard {nullptr};
  Settings mSettings;

  std::shared_ptr<WebView2PageSource> mDelegate;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(BrowserTab::Settings);

}// namespace OpenKneeboard
