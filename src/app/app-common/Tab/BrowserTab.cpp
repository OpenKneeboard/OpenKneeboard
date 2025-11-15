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

#include <OpenKneeboard/json/Geometry2D.hpp>

#include <OpenKneeboard/dprint.hpp>

namespace OpenKneeboard {

BrowserTab::BrowserTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const Settings& settings)
  : TabBase(
      persistentID,
      title.empty() ? std::string_view {_("Web Dashboard")} : title),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs),
    mSettings(settings),
    mHaveTitle(!title.empty()) {
}

task<std::shared_ptr<BrowserTab>> BrowserTab::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  winrt::guid persistentID,
  std::string_view title,
  Settings settings) {
  std::shared_ptr<BrowserTab> ret {
    new BrowserTab(dxr, kbs, persistentID, title, settings)};
  co_await ret->Reload();
  co_return ret;
}

BrowserTab::~BrowserTab() {
  OPENKNEEBOARD_TraceLoggingScope("BrowserTab::~BrowserTab()");
  this->RemoveAllEventListeners();
}

std::string BrowserTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string BrowserTab::GetStaticGlyph() {
  // Website
  return {"\ueb41"};
}

task<void> BrowserTab::Reload() {
  OPENKNEEBOARD_TraceLoggingCoro("BrowserTab::Reload()");
  auto keepAlive = shared_from_this();

  mDelegate = {};
  co_await this->SetDelegates({});
  mDelegate = co_await ChromiumPageSource::Create(
    mDXR, mKneeboard, WebPageSourceKind::WebDashboard, mSettings);
  this->RemoveAllEventListeners();
  AddEventListener(
    mDelegate->evDocumentTitleChangedEvent,
    {
      this,
      [](auto self, auto title) -> fire_and_forget {
        if (self->mHaveTitle) {
          co_return;
        }
        co_await self->mUIThread;
        self->SetTitle(title);
        self->mHaveTitle = true;
      },
    });
  co_await this->SetDelegates({mDelegate});
}

nlohmann::json BrowserTab::GetSettings() const {
  return mSettings;
}

bool BrowserTab::IsSimHubIntegrationEnabled() const {
  return mSettings.mIntegrateWithSimHub;
}

task<void> BrowserTab::SetSimHubIntegrationEnabled(bool enabled) {
  OPENKNEEBOARD_TraceLoggingCoro("BrowserTab::SetSimHubIntegrationEnabled()");
  if (enabled == this->IsSimHubIntegrationEnabled()) {
    co_return;
  }
  mSettings.mIntegrateWithSimHub = enabled;
  co_await this->Reload();
  this->evSettingsChangedEvent.Emit();
}

bool BrowserTab::AreOpenKneeboardAPIsEnabled() const {
  return mSettings.mExposeOpenKneeboardAPIs;
}

task<void> BrowserTab::SetOpenKneeboardAPIsEnabled(const bool enabled) {
  OPENKNEEBOARD_TraceLoggingCoro("BrowserTab::SetOpenKneeboardAPIsEnabled()");
  if (enabled == this->AreOpenKneeboardAPIsEnabled()) {
    co_return;
  }
  mSettings.mExposeOpenKneeboardAPIs = enabled;
  co_await this->Reload();
  this->evSettingsChangedEvent.Emit();
}

bool BrowserTab::IsBackgroundTransparent() const {
  return mSettings.mTransparentBackground;
}

task<void> BrowserTab::SetBackgroundTransparent(bool transparent) {
  OPENKNEEBOARD_TraceLoggingCoro("BrowserTab::SetBackgroundTransparent()");
  if (transparent == this->IsBackgroundTransparent()) {
    co_return;
  }
  mSettings.mTransparentBackground = transparent;
  co_await this->Reload();
  this->evSettingsChangedEvent.Emit();
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  BrowserTab::Settings,
  mURI,
  mInitialSize,
  mIntegrateWithSimHub,
  mTransparentBackground,
  mExposeOpenKneeboardAPIs)

}// namespace OpenKneeboard
