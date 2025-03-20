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
#include <OpenKneeboard/DoodleRenderer.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/version.hpp>

#include <Shlwapi.h>

#include <include/cef_client.h>

#include <wininet.h>

namespace OpenKneeboard {

ChromiumPageSource::~ChromiumPageSource() {
}

task<std::shared_ptr<ChromiumPageSource>> ChromiumPageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  Kind kind,
  Settings settings) {
  std::shared_ptr<ChromiumPageSource> ret(
    new ChromiumPageSource(dxr, kbs, kind, settings));
  ret->Init();
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

CefRefPtr<ChromiumPageSource::Client> ChromiumPageSource::CreateClient(
  std::optional<KneeboardViewID> viewID) {
  CefRefPtr<Client> client = {new Client(shared_from_this(), viewID)};

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
     Client::GetSupportedExperimentalFeatures()},
    {"VirtualHosts", nlohmann::json::object({})},
  };
  extraData->SetString("InitData", initData.dump());
  extraData->SetBool("IntegrateWithSimHub", mSettings.mIntegrateWithSimHub);

  CefBrowserHost::CreateBrowser(
    info, client, mSettings.mURI, settings, extraData, nullptr);

  return client;
}

void ChromiumPageSource::PostCursorEvent(
  KneeboardViewID view,
  const CursorEvent& ev,
  PageID pageID) {
  OPENKNEEBOARD_TraceLoggingScope(
    "ChromiumPageSource::PostCursorEvent()",
    TraceLoggingValue(pageID.GetTemporaryValue(), "PageID"));

  if (this->GetPageCount() == 0) {
    return;
  }

  auto client = this->GetOrCreateClient(view);
  const auto mode = client->GetCursorEventsMode();
  using enum Client::CursorEventsMode;
  if (mode == MouseEmulation) {
    client->PostCursorEvent(ev);
    return;
  }
  OPENKNEEBOARD_ASSERT(mode == DoodlesOnly);
  if (!mDoodles) [[unlikely]] {
    mDoodles = std::make_unique<DoodleRenderer>(mDXResources, mKneeboard);
    AddEventListener(mDoodles->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  }
  const auto size = this->GetPreferredSize(pageID);
  if (!size) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mDoodles->PostCursorEvent(view, ev, pageID, size->mPixelSize);
}

task<void>
ChromiumPageSource::RenderPage(RenderContext rc, PageID id, PixelRect rect) {
  OPENKNEEBOARD_TraceLoggingScope(
    "ChromiumPageSource::RenderPage()",
    TraceLoggingValue(id.GetTemporaryValue(), "PageID"));

  auto client = this->GetOrCreateClient(rc.GetKneeboardView()->GetRuntimeID());

  auto rh = client->GetRenderHandlerSubclass();
  if (rh->mFrameCount == 0) {
    co_return;
  }

  if (id != client->GetCurrentPage()) {
    auto size = this->GetPreferredSize(id);
    if (!size) {
      co_return;
    }
    client->SetCurrentPage(id, size->mPixelSize);
  }

  rh->RenderPage(rc, rect);

  if (mDoodles) {
    mDoodles->Render(rc.GetRenderTarget(), id, rect);
  }

  co_return;
}

bool ChromiumPageSource::CanClearUserInput(PageID pageID) const {
  if (!mDoodles) {
    return false;
  }
  return mDoodles->HaveDoodles(pageID);
}

bool ChromiumPageSource::CanClearUserInput() const {
  if (!mDoodles) {
    return false;
  }
  return mDoodles->HaveDoodles();
}

void ChromiumPageSource::ClearUserInput(PageID pageID) {
  if (!mDoodles) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mDoodles->ClearPage(pageID);
}

void ChromiumPageSource::ClearUserInput() {
  if (!mDoodles) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mDoodles->Clear();
}

PageIndex ChromiumPageSource::GetPageCount() const {
  OPENKNEEBOARD_TraceLoggingScope("ChromiumPageSource::GetPageCount()");

  std::shared_lock lock(mStateMutex);

  if (auto state = get_if<ScrollableState>(&mState)) {
    auto rh = state->mClient->GetRenderHandlerSubclass();
    return (rh->mFrameCount > 0) ? 1 : 0;
  }

  if (auto state = get_if<PageBasedState>(&mState)) {
    return state->mPages.size();
  }

  fatal("Invalid ChromiumPageSource state");
}

std::vector<PageID> ChromiumPageSource::GetPageIDs() const {
  OPENKNEEBOARD_TraceLoggingScope("ChromiumPageSource::GetPageIDs()");
  std::shared_lock lock(mStateMutex);
  if (auto state = get_if<ScrollableState>(&mState)) {
    return {state->mClient->GetCurrentPage()};
  }

  if (auto state = get_if<PageBasedState>(&mState)) {
    return std::ranges::transform_view(
             state->mPages, [](const APIPage& page) { return page.mPageID; })
      | std::ranges::to<std::vector>();
  }

  fatal("Invalid ChromiumPageSource state");
}

std::optional<PreferredSize> ChromiumPageSource::GetPreferredSize(PageID page) {
  OPENKNEEBOARD_TraceLoggingScope("ChromiumPageSource::GetPreferredSize()");
  if (this->GetPageCount() == 0) {
    return std::nullopt;
  }

  std::shared_lock lock(mStateMutex);
  if (auto state = get_if<ScrollableState>(&mState)) {
    auto rh = state->mClient->GetRenderHandlerSubclass();
    const auto& frame = rh->mFrames.at(rh->mFrameCount % rh->mFrames.size());
    return PreferredSize {
      .mPixelSize = frame.mSize,
      .mScalingKind = ScalingKind::Bitmap,
    };
  }

  if (auto state = get_if<PageBasedState>(&mState)) {
    auto it = std::ranges::find(state->mPages, page, &APIPage::mPageID);
    if (it == state->mPages.end()) {
      return std::nullopt;
    }
    return PreferredSize {
      .mPixelSize = it->mPixelSize,
      .mScalingKind = ScalingKind::Bitmap,
    };
  }

  fatal("Invalid ChromiumPageSource state");
}

ChromiumPageSource::ChromiumPageSource(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  Kind kind,
  Settings settings)
  : mDXResources(dxr),
    mKneeboard(kbs),
    mKind(kind),
    mSettings(settings),
    mSpriteBatch(dxr->mD3D11Device.get()) {
}

void ChromiumPageSource::Init() {
  // Not in the constructor because we must be fully initialized and in an
  // std::shared_ptr before we use shared_from_this
  mState = ScrollableState {this->CreateClient()};
}

CefRefPtr<ChromiumPageSource::Client> ChromiumPageSource::GetOrCreateClient(
  KneeboardViewID id) {
  std::shared_lock lock(mStateMutex);

  if (auto state = get_if<ScrollableState>(&mState)) {
    return state->mClient;
  }
  auto state = get_if<PageBasedState>(&mState);
  if (!state) {
    fatal("Invalid ChromiumPageSource state");
  }

  if (state->mClients.contains(id)) {
    return state->mClients.at(id);
  }
  lock.unlock();

  std::unique_lock writeLock(mStateMutex);
  if (state->mClients.empty()) {
    state->mClients.emplace(id, state->mPrimaryClient);
    state->mPrimaryClient->SetViewID(id);
    return state->mPrimaryClient;
  }

  if (!state->mClients.contains(id)) {
    state->mClients.emplace(id, this->CreateClient(id));
  }
  return state->mClients.at(id);
}

void ChromiumPageSource::PostCustomAction(
  KneeboardViewID view,
  std::string_view actionID,
  const nlohmann::json& arg) {
  OPENKNEEBOARD_TraceLoggingScope("ChromiumPageSource::GetPreferredSize()");
  this->GetOrCreateClient(view)->PostCustomAction(actionID, arg);
}

fire_and_forget ChromiumPageSource::OpenDeveloperToolsWindow(
  KneeboardViewID view,
  PageID) {
  co_await GetOrCreateClient(view)->OpenDeveloperToolsWindow();
}

}// namespace OpenKneeboard