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
#include <OpenKneeboard/BrowserTab.hpp>

#include <OpenKneeboard/dprint.hpp>

namespace OpenKneeboard {

BrowserTab::BrowserTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const Settings& settings)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mSettings(settings) {
  fire_and_forget(this->Reload());
}

BrowserTab::~BrowserTab() {
  this->RemoveAllEventListeners();
}

std::string BrowserTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string BrowserTab::GetStaticGlyph() {
  // Website
  return {"\ueb41"};
}

IAsyncAction BrowserTab::Reload() {
  mDelegate = {};
  co_await this->SetDelegates({});
  mDelegate = WebView2PageSource::Create(mDXR, mKneeboard, mSettings);
  co_await this->SetDelegates({mDelegate});
}

nlohmann::json BrowserTab::GetSettings() const {
  return mSettings;
}

bool BrowserTab::IsSimHubIntegrationEnabled() const {
  return mSettings.mIntegrateWithSimHub;
}

IAsyncAction BrowserTab::SetSimHubIntegrationEnabled(bool enabled) {
  if (enabled == this->IsSimHubIntegrationEnabled()) {
    co_return;
  }
  mSettings.mIntegrateWithSimHub = enabled;
  co_await this->Reload();
  this->evSettingsChangedEvent.Emit();
}

bool BrowserTab::IsBackgroundTransparent() const {
  return mSettings.mTransparentBackground;
}

IAsyncAction BrowserTab::SetBackgroundTransparent(bool transparent) {
  OPENKNEEBOARD_TraceLoggingCoro("BrowserTab::SetBackgroundTransparent()");
  if (transparent == this->IsBackgroundTransparent()) {
    co_return;
  }
  mSettings.mTransparentBackground = transparent;
  co_await this->Reload();
  this->evSettingsChangedEvent.Emit();
}

bool BrowserTab::IsDeveloperToolsWindowEnabled() const {
  return mSettings.mOpenDeveloperToolsWindow;
}

IAsyncAction BrowserTab::SetDeveloperToolsWindowEnabled(bool enabled) {
  OPENKNEEBOARD_TraceLoggingCoro(
    "BrowserTab::SetDeveloperToolsWindowsEnabled()");
  if (enabled == this->IsDeveloperToolsWindowEnabled()) {
    co_return;
  }
  mSettings.mOpenDeveloperToolsWindow = enabled;
  co_await this->Reload();
  this->evSettingsChangedEvent.Emit();
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  BrowserTab::Settings,
  mURI,
  mInitialSize,
  mIntegrateWithSimHub,
  mOpenDeveloperToolsWindow,
  mTransparentBackground)

}// namespace OpenKneeboard
