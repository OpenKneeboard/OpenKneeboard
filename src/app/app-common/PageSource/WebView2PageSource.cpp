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

#include <OpenKneeboard/config.h>

#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/WebView2PageSource.h>

#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/hresult.h>
#include <OpenKneeboard/json_format.h>
#include <OpenKneeboard/version.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <fstream>
#include <sstream>
#include <system_error>

#include <WebView2.h>
#include <wrl.h>

namespace OpenKneeboard {

std::shared_ptr<WebView2PageSource> WebView2PageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings) {
  auto ret
    = shared_with_final_release(new WebView2PageSource(dxr, kbs, settings));
  ret->Init();
  return ret;
}

winrt::fire_and_forget WebView2PageSource::Init() {
  if (!IsAvailable()) {
    co_return;
  }
  auto uiThread = mUIThread;
  auto weak = weak_from_this();
  auto dq = mDQC.DispatcherQueue();
  co_await winrt::resume_foreground(dq);
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  mWorkerThread = {};
  SetThreadDescription(GetCurrentThread(), L"OKB WebView2 Worker");

  using namespace winrt::Microsoft::Web::WebView2::Core;

  const auto userData = Filesystem::GetLocalAppDataDirectory() / "WebView2";
  std::filesystem::create_directories(userData);

  const auto edgeArgs
    = std::format(L"--disable-gpu-vsync --max-gum-fps={}", FramesPerSecond);
  CoreWebView2EnvironmentOptions options;
  options.AdditionalBrowserArguments(edgeArgs);

  mEnvironment = co_await CoreWebView2Environment::CreateWithOptionsAsync(
    {}, userData.wstring(), options);

  co_await uiThread;

  auto doodles = std::make_shared<DoodleRenderer>(mDXResources, mKneeboard);

  APIPage apiPage {
    .mGuid = random_guid(),
    .mPixelSize = mSettings.mInitialSize,
  };
  mDocumentResources = {
    .mPages = {apiPage},
    .mDoodles = doodles,
  };
}

WebView2PageSource::WebView2PageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings)
  : mDXResources(dxr), mKneeboard(kbs), mSettings(settings) {
  OPENKNEEBOARD_TraceLoggingScope("WebView2PageSource::WebView2PageSource()");
  if (!IsAvailable()) {
    return;
  }
  mDQC = winrt::Windows::System::DispatcherQueueController::
    CreateOnDedicatedThread();
}

std::string WebView2PageSource::GetVersion() {
  static std::string sVersion;
  static std::once_flag sFlag;
  std::call_once(sFlag, [p = &sVersion]() {
    WCHAR* version {nullptr};
    if (FAILED(
          GetAvailableCoreWebView2BrowserVersionString(nullptr, &version))) {
      return;
    }

    *p = winrt::to_string(version);

    CoTaskMemFree(version);
  });
  return sVersion;
}

bool WebView2PageSource::IsAvailable() {
  return !GetVersion().empty();
}

WebView2PageSource::~WebView2PageSource() = default;

winrt::fire_and_forget WebView2PageSource::final_release(
  std::unique_ptr<WebView2PageSource> self) {
  if (self->mWorkerThread) {
    co_await self->mWorkerThread;

    self->mEnvironment = nullptr;
    self->mWorkerThread = {nullptr};
  }
  co_await self->mUIThread;
  co_await self->mDQC.ShutdownQueueAsync();
  co_await self->mUIThread;
  co_return;
}

void WebView2PageSource::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& event,
  PageID pageID) {
  // FIXME: delegate to correct WebView2Renderer
}

bool WebView2PageSource::CanClearUserInput(PageID pageID) const {
  const auto& doodles = mDocumentResources.mDoodles;
  return doodles && doodles->HaveDoodles(pageID);
}

bool WebView2PageSource::CanClearUserInput() const {
  const auto& doodles = mDocumentResources.mDoodles;
  return doodles && doodles->HaveDoodles();
}

void WebView2PageSource::ClearUserInput(PageID pageID) {
  auto& doodles = mDocumentResources.mDoodles;
  if (!doodles) {
    return;
  }
  doodles->ClearPage(pageID);
}

void WebView2PageSource::ClearUserInput() {
  auto& doodles = mDocumentResources.mDoodles;
  if (!doodles) {
    return;
  }
  doodles->Clear();
}

PageIndex WebView2PageSource::GetPageCount() const {
  return this->GetPageIDs().size();
}

std::vector<PageID> WebView2PageSource::GetPageIDs() const {
  switch (mDocumentResources.mContentMode) {
    case ContentMode::PageBased:
      return mDocumentResources.mPages
        | std::views::transform(&APIPage::mPageID)
        | std::ranges::to<std::vector>();
    case ContentMode::Scrollable:
      return {mScrollableContentPageID};
  }
  OPENKNEEBOARD_LOG_AND_FATAL(
    "Invalid content mode in WebView2PageSource::GetPageIDs()");
}

PreferredSize WebView2PageSource::GetPreferredSize(PageID pageID) {
  auto it
    = std::ranges::find(mDocumentResources.mPages, pageID, &APIPage::mPageID);
  if (it != mDocumentResources.mPages.end()) {
    return {
      it->mPixelSize,
      ScalingKind::Bitmap,
    };
  }
  return {};
}

void WebView2PageSource::RenderPage(
  const RenderContext& rc,
  PageID pageID,
  const PixelRect& rect) {
  const auto isPageMode
    = (mDocumentResources.mContentMode == ContentMode::PageBased);

  const auto view = rc.GetKneeboardView();
  if (!view) {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "WebView2PageSource::Render() should always have a view");
  }

  const auto key
    = isPageMode ? view->GetRuntimeID() : mScrollableContentRendererKey;

  if (!mDocumentResources.mRenderers.contains(key)) {
    auto renderer = WebView2Renderer::Create(
      mDXResources,
      mKneeboard,
      mSettings,
      mDocumentResources.mDoodles,
      mDQC,
      mEnvironment,
      (isPageMode ? view : nullptr),
      (isPageMode ? mDocumentResources.mPages
                  : decltype(mDocumentResources.mPages) {}));
    AddEventListener(
      renderer->evContentChangedEvent, this->evContentChangedEvent);
    AddEventListener(
      renderer->evJSAPI_SetPages,
      {weak_from_this(), &WebView2PageSource::OnJSAPI_SetPages});
    AddEventListener(
      renderer->evJSAPI_SendMessageToPeers,
      {weak_from_this(), &WebView2PageSource::OnJSAPI_SendMessageToPeers});
    AddEventListener(renderer->evNeedsRepaintEvent, this->evNeedsRepaintEvent);

    mDocumentResources.mRenderers.emplace(key, std::move(renderer));
  }
  auto renderer = mDocumentResources.mRenderers.at(key);
  renderer->RenderPage(rc, pageID, rect);
}

void WebView2PageSource::OnJSAPI_SetPages(const std::vector<APIPage>& pages) {
  mDocumentResources.mPages = pages;

  for (const auto& [rtid, renderer]: mDocumentResources.mRenderers) {
    renderer->OnJSAPI_Peer_SetPages(pages);
  }

  if (mDocumentResources.mContentMode != ContentMode::PageBased) {
    mDocumentResources.mContentMode = ContentMode::PageBased;
    mDocumentResources.mRenderers.erase(mScrollableContentRendererKey);
  }

  evContentChangedEvent.Emit();
  evAvailableFeaturesChangedEvent.Emit();
}

void WebView2PageSource::OnJSAPI_SendMessageToPeers(
  const InstanceID& sender,
  const nlohmann::json& message) {
  for (const auto& [rtid, renderer]: mDocumentResources.mRenderers) {
    renderer->OnJSAPI_Peer_SendMessageToPeers(sender, message);
  }
}

}// namespace OpenKneeboard
