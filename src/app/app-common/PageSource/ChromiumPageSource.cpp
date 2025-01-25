/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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

#include "ChromiumPageSource_Client.hpp"
#include "ChromiumPageSource_RenderHandler.hpp"

#include <OpenKneeboard/ChromiumPageSource.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/version.hpp>

#include <Shlwapi.h>

#include <include/cef_client.h>

#include <wininet.h>

namespace OpenKneeboard {

ChromiumPageSource::~ChromiumPageSource() = default;

task<std::shared_ptr<ChromiumPageSource>> ChromiumPageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  Kind kind,
  Settings settings) {
  std::shared_ptr<ChromiumPageSource> ret(
    new ChromiumPageSource(dxr, kbs, kind, settings));
  co_await ret->Init();
  co_return ret;
}

task<std::shared_ptr<ChromiumPageSource>> ChromiumPageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  std::filesystem::path path) {
  char buffer[INTERNET_MAX_URL_LENGTH];
  DWORD charCount {std::size(buffer)};
  check_hresult(
    UrlCreateFromPathA(path.string().c_str(), buffer, &charCount, NULL));
  const Settings settings {
    .mIntegrateWithSimHub = false,
    .mURI = {buffer, charCount},
  };
  return Create(dxr, kbs, Kind::File, settings);
}

task<void> ChromiumPageSource::Init() {
  mClient = {new Client(this)};

  CefWindowInfo info {};
  info.SetAsWindowless(nullptr);
  info.shared_texture_enabled = true;

  CefBrowserSettings settings;
  settings.windowless_frame_rate = Config::FramesPerSecond;
  if (mSettings.mTransparentBackground) {
    settings.background_color = CefColorSetARGB(0x00, 0x00, 0x00, 0x00);
  }

  auto extraData = CefDictionaryValue::Create();

  nlohmann::json initData {
    {
      "Version",
      {
        {
          "Components",
          {
            {"Major", Version::Major},
            {"Minor", Version::Minor},
            {"Patch", Version::Patch},
            {"Build", Version::Build},
          },
        },
        {"HumanReadable", Version::ReleaseName},
        {"IsGitHubActionsBuild", Version::IsGithubActionsBuild},
        {"IsTaggedVersion", Version::IsTaggedVersion},
        {"IsStableRelease", Version::IsStableRelease},
      },
    },
    {"AvailableExperimentalFeatures",
     nlohmann::json::array({}) /* SupportedExperimentalFeatures */},
    {"VirtualHosts", nlohmann::json::object({})},
  };
  extraData->SetString("InitData", initData.dump());

  CefBrowserHost::CreateBrowser(
    info, mClient, mSettings.mURI, settings, extraData, nullptr);
  co_return;
}

task<void> ChromiumPageSource::DisposeAsync() noexcept {
  auto disposing = co_await mDisposal.StartOnce();
  co_return;
}

void ChromiumPageSource::PostCursorEvent(
  KneeboardViewID,
  const CursorEvent& ev,
  PageID) {
  if (this->GetPageCount() == 0) {
    return;
  }
  mClient->PostCursorEvent(ev);
}

task<void>
ChromiumPageSource::RenderPage(RenderContext rc, PageID id, PixelRect rect) {
  auto rh = mClient->GetRenderHandlerSubclass();
  if (id != mPageID || rh->mFrameCount == 0) {
    co_return;
  }

  rh->RenderPage(rc, rect);

  co_return;
}

bool ChromiumPageSource::CanClearUserInput(PageID) const {
  return CanClearUserInput();
}

bool ChromiumPageSource::CanClearUserInput() const {
  // TODO
  return false;
}

void ChromiumPageSource::ClearUserInput(PageID) {
  ClearUserInput();
}

void ChromiumPageSource::ClearUserInput() {
  // TODO
}

PageIndex ChromiumPageSource::GetPageCount() const {
  if (!mClient) {
    return 0;
  }
  auto rh = mClient->GetRenderHandlerSubclass();
  if (!rh) {
    return 0;
  }
  return (rh->mFrameCount > 0) ? 1 : 0;
}

std::vector<PageID> ChromiumPageSource::GetPageIDs() const {
  if (this->GetPageCount() == 0) {
    return {};
  }
  return {mPageID};
}

std::optional<PreferredSize> ChromiumPageSource::GetPreferredSize(PageID) {
  if (this->GetPageCount() == 0) {
    return std::nullopt;
  }

  auto rh = this->mClient->GetRenderHandlerSubclass();
  const auto& frame = rh->mFrames.at(rh->mFrameCount % rh->mFrames.size());
  return PreferredSize {
    .mPixelSize = frame.mSize,
    .mScalingKind = ScalingKind::Bitmap,
  };
}

ChromiumPageSource::ChromiumPageSource(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  Kind,
  Settings settings)
  : mDXResources(dxr),
    mKneeboard(kbs),
    mSettings(settings),
    mSpriteBatch(dxr->mD3D11Device.get()) {
}

}// namespace OpenKneeboard