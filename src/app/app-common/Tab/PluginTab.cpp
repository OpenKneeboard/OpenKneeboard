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

#include <Shlwapi.h>

#include <wininet.h>

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
}

task<std::shared_ptr<PluginTab>> PluginTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const Settings& settings) {
  std::shared_ptr<PluginTab> ret {
    new PluginTab(dxr, kbs, persistentID, title, settings)};
  co_await ret->Reload();
  co_return ret;
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

nlohmann::json PluginTab::GetSettings() const {
  return mSettings;
}

task<void> PluginTab::Reload() {
  const auto disposable
    = std::dynamic_pointer_cast<IHasDisposeAsync>(mDelegate);
  if (disposable) {
    co_await disposable->DisposeAsync();
  }
  mDelegate = nullptr;
  mTabType = std::nullopt;
  co_await this->SetDelegates({});

  Plugin plugin {};
  for (auto&& it: mKneeboard->GetPluginStore()->GetPlugins()) {
    for (auto&& tabType: it.mTabTypes) {
      if (tabType.mID == mSettings.mPluginTabTypeID) {
        mTabType = tabType;
        plugin = it;
        break;
      }
    }
  }

  if (!mTabType) {
    dprint(
      "WARNING: couldn't find plugin and implementation for tab type `{}`",
      mSettings.mPluginTabTypeID);
    co_return;
  }

  using Kind = Plugin::TabType::Implementation;
  switch (mTabType->mImplementation) {
    case Kind::WebBrowser: {
      const auto args = std::get<Plugin::TabType::WebBrowserArgs>(
        mTabType->mImplementationArgs);
      auto uri = args.mURI;
      const std::string_view pluginScheme {"plugin://"};
      if (uri.starts_with(pluginScheme)) {
        char buffer[INTERNET_MAX_URL_LENGTH];
        DWORD charCount {std::size(buffer)};
        winrt::check_hresult(UrlCreateFromPathA(
          plugin.mJSONPath.parent_path().string().c_str(),
          buffer,
          &charCount,
          NULL));
        uri.replace(
          0,
          pluginScheme.size(),
          std::format("{}/", std::string_view {buffer, charCount}));
      }
      mDelegate = co_await WebView2PageSource::Create(
        mDXResources,
        mKneeboard,
        WebView2PageSource::Kind::Plugin,
        WebView2PageSource::Settings {
          .mInitialSize = args.mInitialSize,
          .mIntegrateWithSimHub = false,
          .mURI = uri,
        });
      co_await this->SetDelegates({mDelegate});
      co_return;
    }
    default:
      dprint("Unrecognized plugin implementation");
      OPENKNEEBOARD_BREAK;
      co_return;
  }
}

void PluginTab::PostCustomAction(
  KneeboardViewID viewID,
  std::string_view id,
  const nlohmann::json& arg) {
  if (!mTabType) {
    return;
  }

  if (!id.starts_with(mTabType->mID + ";")) {
    return;
  }

  const auto actions = mTabType->mCustomActions;
  const auto it = std::ranges::find(actions, id, &Plugin::CustomAction::mID);
  if (it == actions.end()) {
    dprint(
      "Action ID `{}` seems to be for tab `{}`, but action ID is not "
      "recognized",
      mTabType->mID,
      id);
    return;
  }

  const auto postable
    = std::dynamic_pointer_cast<WebView2PageSource>(mDelegate);
  if (postable) {
    postable->PostCustomAction(viewID, id, arg);
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(PluginTab::Settings, mPluginTabTypeID);

}// namespace OpenKneeboard