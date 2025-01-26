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

#include <OpenKneeboard/ChromiumPageSource.hpp>
#include <OpenKneeboard/D2DErrorRenderer.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/PluginStore.hpp>
#include <OpenKneeboard/PluginTab.hpp>

#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/semver.hpp>
#include <OpenKneeboard/version.hpp>

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
  mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr);
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

  mState = State::Uninit;
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
    mState = State::PluginNotFound;
    dprint.Error(
      "Couldn't find plugin and implementation for tab type `{}`",
      mSettings.mPluginTabTypeID);
    co_return;
  }

  if (
    CompareVersions(plugin.mMetadata.mOKBMinimumVersion, Version::ReleaseName)
    == ThreeWayCompareResult::GreaterThan) {
    dprint.Warning("OpenKneeboard is too old for plugin `{}`", plugin.mID);
    mState = State::OpenKneeboardTooOld;
    co_return;
  }

  using Kind = Plugin::TabType::Implementation;
  switch (mTabType->mImplementation) {
    case Kind::WebBrowser: {
      const auto args = std::get<Plugin::TabType::WebBrowserArgs>(
        mTabType->mImplementationArgs);
      WebPageSourceSettings settings {
        .mInitialSize = args.mInitialSize,
        .mIntegrateWithSimHub = false,
        .mURI = args.mURI,
      };

      const std::string_view pluginScheme {"plugin://"};
      if (settings.mURI.starts_with(pluginScheme)) {
        const auto vhost = std::format(
          "{}.openkneeboardplugins.localhost", plugin.GetIDHash());
        const auto path = plugin.mJSONPath.parent_path();
        settings.mVirtualHosts.emplace(vhost, path);
        settings.mURI.replace(
          0, pluginScheme.size(), std::format("https://{}/", vhost));
        dprint(
          "🧩 Serving plugin '{}' from `https://{}` => `{}`",
          plugin.mID,
          vhost,
          path);
      }

      mDelegate = co_await ChromiumPageSource::Create(
        mDXResources, mKneeboard, WebPageSourceKind::Plugin, settings);
      co_await this->SetDelegates({mDelegate});
      mState = State::OK;
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
    = std::dynamic_pointer_cast<ChromiumPageSource>(mDelegate);
  if (postable) {
    postable->PostCustomAction(viewID, id, arg);
  }
}

std::string PluginTab::GetPluginTabTypeID() const noexcept {
  return mSettings.mPluginTabTypeID;
}

PageIndex PluginTab::GetPageCount() const {
  if (mState == State::OK) {
    return PageSourceWithDelegates::GetPageCount();
  }
  return 1;
}

task<void>
PluginTab::RenderPage(RenderContext ctx, PageID page, PixelRect rect) {
  switch (mState) {
    case State::OK:
      co_await PageSourceWithDelegates::RenderPage(ctx, page, rect);
      co_return;
    case State::Uninit:
      // Shouldn't get here :/
      mErrorRenderer->Render(ctx.d2d(), _("💩"), rect);
      co_return;
    case State::PluginNotFound:
      mErrorRenderer->Render(ctx.d2d(), _("Plugin Not Installed"), rect);
      co_return;
    case State::OpenKneeboardTooOld:
      mErrorRenderer->Render(
        ctx.d2d(), _("Plugin Requires Newer OpenKneeboard"), rect);
      co_return;
  }
  std::unreachable();
}

bool PluginTab::HasDeveloperTools(PageID pageID) const {
  auto delegate
    = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(mDelegate);
  if (delegate) {
    return delegate->HasDeveloperTools(pageID);
  }
  return false;
}

fire_and_forget PluginTab::OpenDeveloperToolsWindow(
  KneeboardViewID view,
  PageID page) {
  auto delegate
    = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(mDelegate);
  if (delegate) {
    delegate->OpenDeveloperToolsWindow(view, page);
  }
  co_return;
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(PluginTab::Settings, mPluginTabTypeID);

}// namespace OpenKneeboard