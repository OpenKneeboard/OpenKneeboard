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

#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/PluginStore.hpp>
#include <OpenKneeboard/PluginTab.hpp>
#include <OpenKneeboard/WebView2PageSource.hpp>

namespace OpenKneeboard {

PluginTab::PluginTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const Settings& settings)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXResources(dxr),
    mKneeboard(kbs),
    mSettings(settings) {
  this->LoadFromEmpty();
}

PluginTab::~PluginTab() {
}

std::string PluginTab::GetGlyph() const {
  static const std::string sDefault = "\uea86";// puzzle

  if (!mTabType) {
    return sDefault;
  }

  if (mTabType->mGlyph.empty()) {
    return sDefault;
  }

  return mTabType->mGlyph;
}

IAsyncAction PluginTab::Reload() {
  const auto disposable
    = std::dynamic_pointer_cast<IHasDisposeAsync>(mDelegate);
  if (disposable) {
    co_await disposable->DisposeAsync();
  }
  mDelegate = nullptr;
  mTabType = std::nullopt;
  co_await this->SetDelegates({});
  this->LoadFromEmpty();
}

nlohmann::json PluginTab::GetSettings() const {
  return mSettings;
}

void PluginTab::LoadFromEmpty() {
  const auto tabTypes = mKneeboard->GetPluginStore()->GetTabTypes();
  const auto it = std::ranges::find(
    tabTypes, mSettings.mPluginTabTypeID, &Plugin::TabType::mID);

  if (it == tabTypes.end()) {
    dprintf(
      "WARNING: couldn't find plugin for tab type `{}`",
      mSettings.mPluginTabTypeID);
    return;
  }

  mTabType = *it;
  using Kind = Plugin::TabType::Implementation;
  switch (it->mImplementation) {
    case Kind::WebBrowser: {
      const auto args
        = std::get<Plugin::TabType::WebBrowserArgs>(it->mImplementationArgs);
      mDelegate = WebView2PageSource::Create(
        mDXResources,
        mKneeboard,
        WebView2PageSource::Settings {
          .mInitialSize = args.mInitialSize,
          .mIntegrateWithSimHub = false,
          .mURI = args.mURI,
        });
      this->SetDelegatesFromEmpty({mDelegate});
      return;
    }
    default:
      dprint("Unrecognized plugin implementation");
      OPENKNEEBOARD_BREAK;
      return;
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(PluginTab::Settings, mPluginTabTypeID);

}// namespace OpenKneeboard