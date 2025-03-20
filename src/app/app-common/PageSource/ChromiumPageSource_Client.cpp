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
#include "ChromiumPageSource_Client.hpp"

#include "ChromiumPageSource_RenderHandler.hpp"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TabletInfo.hpp>
#include <OpenKneeboard/TabletInputAdapter.hpp>

#include <OpenKneeboard/json/Geometry2D.hpp>

#include <OpenKneeboard/json.hpp>

#include <FredEmmott/magic_json_serialize_enum.hpp>
#include <include/cef_parser.h>
#include <include/wrapper/cef_stream_resource_handler.h>

template <>
struct std::formatter<OpenKneeboard::ExperimentalFeature, char>
  : std::formatter<std::string, char> {
  auto format(
    const OpenKneeboard::ExperimentalFeature& feature,
    auto& formatContext) const {
    return std::formatter<std::string, char>::format(
      std::format("`{}` version `{}`", feature.mName, feature.mVersion),
      formatContext);
  }
};

namespace OpenKneeboard {

OPENKNEEBOARD_DEFINE_JSON(ExperimentalFeature, mName, mVersion);
FREDEMMOTT_MAGIC_JSON_SERIALIZE_ENUM(
  ChromiumPageSource::Client::CursorEventsMode);

void to_json(nlohmann::json& j, const ChromiumPageSource::APIPage& v) {
  j.update({
    {"guid", std::format("{:nobraces}", v.mGuid)},
    {"pixelSize",
     {
       {"width", v.mPixelSize.mWidth},
       {"height", v.mPixelSize.mHeight},
     }},
    {"extraData", v.mExtraData},
  });
}

void from_json(const nlohmann::json& j, ChromiumPageSource::APIPage& v) {
  v.mGuid = j.at("guid");
  if (j.contains("extraData")) {
    v.mExtraData = j.at("extraData");
  }
  if (j.contains("pixelSize")) {
    const auto ps = j.at("pixelSize");
    v.mPixelSize = PixelSize {
      ps.at("width"),
      ps.at("height"),
    };
  }
}

namespace {
using JSAPIResult = std::expected<nlohmann::json, std::string>;

template <class... Args>
auto jsapi_error(std::format_string<Args...> fmt, Args&&... args) {
  return std::unexpected {std::format(fmt, std::forward<Args>(args)...)};
}

auto jsapi_missing_feature_error(const ExperimentalFeature& feature) {
  return jsapi_error("Missing required experimental feature:{}", feature);
}

template <class T>
struct method_reflection;

template <class R, class O, class... Args>
struct method_reflection<R (O::*)(Args...)> {
  using args_tuple = std::tuple<Args...>;
  static constexpr std::size_t args_count = sizeof...(Args);
};

template <auto TMethod>
task<JSAPIResult> JSInvoke(auto self, nlohmann::json args) {
  using Reflection = method_reflection<decltype(TMethod)>;
  using ArgsTuple = Reflection::args_tuple;
  constexpr auto ArgCount = Reflection::args_count;

  if (!args.is_array()) {
    co_return jsapi_error("Native API call arguments must be an array");
  }
  if (args.size() != ArgCount) {
    co_return jsapi_error(
      "Native API call required {} arguments, {} provided",
      ArgCount,
      args.size());
  }

  const auto argsTuple = [&]<std::size_t... I>(std::index_sequence<I...>)
    -> std::expected<ArgsTuple, nlohmann::json::exception> {
    try {
      return ArgsTuple {
        args.at(I).get<std::tuple_element_t<I, ArgsTuple>>()...};
    } catch (const nlohmann::json::exception& e) {
      return std::unexpected(e);
    }
  }(std::make_index_sequence<ArgCount> {});
  if (!argsTuple.has_value()) {
    co_return jsapi_error(
      "Error decoding native API arguments: {}", argsTuple.error().what());
  }

  co_return co_await std::apply(std::bind_front(TMethod, self), *argsTuple);
}

const ExperimentalFeature DoodlesOnlyFeature {"DoodlesOnly", 2024071802};

const ExperimentalFeature SetCursorEventsModeFeature {
  "SetCursorEventsMode",
  2024071801};

const ExperimentalFeature PageBasedContentFeature {
  "PageBasedContent",
  2024072001};
const ExperimentalFeature PageBasedContentWithRequestPageChangeFeature {
  "PageBasedContent",
  2024073001};

const ExperimentalFeature GraphicsTabletInfoFeature {
  "GraphicsTabletInfo",
  2025012901};

const std::array SupportedExperimentalFeatures {
  DoodlesOnlyFeature,
  SetCursorEventsModeFeature,
  PageBasedContentFeature,
  PageBasedContentWithRequestPageChangeFeature,
  GraphicsTabletInfoFeature,
};

}// namespace

ChromiumPageSource::Client::Client(
  std::shared_ptr<ChromiumPageSource> pageSource,
  std::optional<KneeboardViewID> viewID)
  : mPageSource(pageSource), mViewID(viewID) {
  mRenderHandler = {new RenderHandler(pageSource)};
  mShutdownEvent.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
}

ChromiumPageSource::Client::~Client() {
  if (mBrowser) {
    mBrowser->GetHost()->CloseBrowser(/* force = */ true);
  }
  WaitForSingleObject(mShutdownEvent.get(), INFINITE);
}

void ChromiumPageSource::Client::OnBeforeClose(CefRefPtr<CefBrowser>) {
  OPENKNEEBOARD_TraceLoggingScope(
    "ChromiumPageSource::Client::OnBeforeClose()");
  const FatalOnUncaughtExceptions exceptionBoundary;
  mBrowser = nullptr;
  mRenderHandler = nullptr;
  SetEvent(mShutdownEvent.get());
}

CefRefPtr<CefLifeSpanHandler> ChromiumPageSource::Client::GetLifeSpanHandler() {
  return this;
}

CefRefPtr<CefRenderHandler> ChromiumPageSource::Client::GetRenderHandler() {
  return mRenderHandler;
}

CefRefPtr<ChromiumPageSource::RenderHandler>
ChromiumPageSource::Client::GetRenderHandlerSubclass() {
  return mRenderHandler;
}

CefRefPtr<CefDisplayHandler> ChromiumPageSource::Client::GetDisplayHandler() {
  return this;
}

CefRefPtr<CefRequestHandler> ChromiumPageSource::Client::GetRequestHandler() {
  return this;
}

CefRefPtr<CefResourceRequestHandler>
ChromiumPageSource::Client::GetResourceRequestHandler(
  CefRefPtr<CefBrowser> browser,
  CefRefPtr<CefFrame> frame,
  CefRefPtr<CefRequest> request,
  bool is_navigation,
  bool is_download,
  const CefString& request_initiator,
  bool& disable_default_handling) {
  return this;
}

CefRefPtr<CefResourceHandler> ChromiumPageSource::Client::GetResourceHandler(
  CefRefPtr<CefBrowser> browser,
  CefRefPtr<CefFrame> frame,
  CefRefPtr<CefRequest> request) {
  const FatalOnUncaughtExceptions exceptionBoundary;
  auto make404 = []() {
    constexpr std::string_view content {"404 Not Found"};
    return new CefStreamResourceHandler(
      404,
      "Not found",
      "text/plain",
      {},
      CefStreamReader::CreateForData(
        const_cast<char*>(content.data()), content.size()));
  };
  const auto url = request->GetURL();
  CefURLParts parts {};
  CefParseURL(url, parts);
  const auto scheme = CefString(&parts.scheme).ToString();
  if (scheme != "https" && scheme != "http") {
    return nullptr;
  }
  const auto host = CefString(&parts.host).ToString();

  const auto pageSource = mPageSource.lock();
  if (!pageSource) {
    return nullptr;
  }

  const auto& vhosts = pageSource->mSettings.mVirtualHosts;
  auto it = std::ranges::find(
    vhosts, host, [](const auto& pair) { return pair.first; });
  if (it == vhosts.end()) {
    return nullptr;
  }
  auto relativePath = CefString(&parts.path).ToString();
  if (relativePath.starts_with('/')) {
    relativePath = relativePath.substr(1);
  }
  const auto path = it->second / relativePath;

  const auto ext = path.extension().wstring();
  if (!ext.starts_with(L'.')) {
    return make404();
  }
  const auto mime = CefGetMimeType(ext.substr(1));

  if (!std::filesystem::exists(path)) {
    return make404();
  }

  auto reader = CefStreamReader::CreateForFile(path.wstring());
  return new CefStreamResourceHandler(mime, reader);
}

bool ChromiumPageSource::Client::OnProcessMessageReceived(
  CefRefPtr<CefBrowser> browser,
  CefRefPtr<CefFrame> frame,
  CefProcessId process,
  CefRefPtr<CefProcessMessage> message) {
  const FatalOnUncaughtExceptions exceptionBoundary;
  const auto name = message->GetName().ToString();
  OPENKNEEBOARD_TraceLoggingScope(
    "ChromiumPageSource::Client::OnProcessMessageReceived()",
    TraceLoggingValue(name.c_str(), "MessageName"));
  if (name == "okb/onContextReleased") {
    mCurrentPage = {};
    mEnabledExperimentalFeatures = {};
    return true;
  }
#define IMPLEMENT_JS_API(X) \
  if (name == "okbjs/" #X) { \
    this->OnJSAsyncRequest<&Client::X>(frame, process, message); \
    return true; \
  }
  IMPLEMENT_JS_API(EnableExperimentalFeatures)
  IMPLEMENT_JS_API(GetPages)
  IMPLEMENT_JS_API(OpenDeveloperToolsWindow)
  IMPLEMENT_JS_API(RequestPageChange)
  IMPLEMENT_JS_API(SendMessageToPeers)
  IMPLEMENT_JS_API(SetCursorEventsMode)
  IMPLEMENT_JS_API(SetPages)
  IMPLEMENT_JS_API(SetPreferredPixelSize)
  IMPLEMENT_JS_API(GetGraphicsTabletInfo)
#undef IMPLEMENT_JS_API

  return CefClient::OnProcessMessageReceived(browser, frame, process, message);
}

bool ChromiumPageSource::Client::OnBeforePopup(
  CefRefPtr<CefBrowser> browser,
  CefRefPtr<CefFrame> frame,
  int popup_id,
  const CefString& target_url,
  const CefString& target_frame_name,
  CefLifeSpanHandler::WindowOpenDisposition target_disposition,
  bool user_gesture,
  const CefPopupFeatures& popupFeatures,
  CefWindowInfo& windowInfo,
  CefRefPtr<CefClient>& client,
  CefBrowserSettings& settings,
  CefRefPtr<CefDictionaryValue>& extra_info,
  bool* no_javascript_access) {
  dprint.Warning(
    "CEF - blocking popup: {} in frame '{}'",
    target_url.ToString(),
    target_frame_name.ToString());

  auto message = CefProcessMessage::Create("okbEvent/console.warn");
  message->GetArgumentList()->SetString(
    0,
    std::format(
      "OpenKneeboard does not support popups; requested popup: {}",
      target_url.ToString()));
  frame->SendProcessMessage(PID_RENDERER, message);

  return /* cancel popup creation */ true;
}

template <auto TMethod>
fire_and_forget ChromiumPageSource::Client::OnJSAsyncRequest(
  CefRefPtr<CefFrame> frame,
  CefProcessId process,
  CefRefPtr<CefProcessMessage> message) {
  const auto callID = message->GetArgumentList()->GetInt(0);
  const auto args = nlohmann::json::parse(
    message->GetArgumentList()->GetString(1).ToString());

  this->SendJSAsyncResult(
    frame, process, callID, co_await JSInvoke<TMethod>(this, args));
}

void ChromiumPageSource::Client::SendJSAsyncResult(
  CefRefPtr<CefFrame> frame,
  CefProcessId process,
  int callID,
  JSAPIResult result) {
  auto data = nlohmann::json::object({});
  if (result.has_value()) {
    data.emplace("result", result->is_null() ? "ok" : result.value());
  } else {
    data.emplace("error", result.error());
    dprint.Warning("JS API error: {}", result.error());
  }

  auto message = CefProcessMessage::Create("okb/asyncResult");
  auto args = message->GetArgumentList();
  args->SetInt(0, callID);
  args->SetString(1, data.dump());
  frame->SendProcessMessage(process, message);
}

void ChromiumPageSource::Client::EnableJSAPI(CefString name) {
  auto message = CefProcessMessage::Create("okbEvent/enableAPI");
  auto args = message->GetArgumentList();
  args->SetString(0, name);
  mBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
}

void ChromiumPageSource::Client::OnTitleChange(
  CefRefPtr<CefBrowser>,
  const CefString& title) {
  if (auto it = mPageSource.lock()) {
    it->evDocumentTitleChangedEvent.EnqueueForContext(mUIThread, title);
  }
}

void ChromiumPageSource::Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  OPENKNEEBOARD_TraceLoggingScope(
    "ChromiumPageSource::Client::OnAfterCreated()");
  const FatalOnUncaughtExceptions exceptionBoundary;
  mBrowser = browser;
  mBrowserId = mBrowser->GetIdentifier();
}

CefRefPtr<CefBrowser> ChromiumPageSource::Client::GetBrowser() const {
  return mBrowser;
}

std::optional<int> ChromiumPageSource::Client::GetBrowserID() const {
  return mBrowserId;
}

void ChromiumPageSource::Client::PostCursorEvent(const CursorEvent& ev) {
  OPENKNEEBOARD_TraceLoggingScope(
    "ChromiumPageSource::Client::PostCursorEvent()");
  mLastCursorEventAt = std::chrono::steady_clock::now();
  if (!this->GetBrowser()) {
    return;
  }

  auto host = this->GetBrowser()->GetHost();
  if (!host) {
    return;
  }

  const auto epsilon = std::numeric_limits<decltype(ev.mX)>::epsilon();
  if (ev.mX < epsilon || ev.mY < epsilon) {
    if (!mIsHovered) {
      return;
    }
    mIsHovered = false;
    host->SendMouseMoveEvent({}, /* mouseLeave = */ true);
    return;
  }

  mIsHovered = true;
  CefMouseEvent cme;
  cme.x = static_cast<int>(std::lround(ev.mX));
  cme.y = static_cast<int>(std::lround(ev.mY));

  constexpr uint32_t LeftButton = (1 << 0);
  constexpr uint32_t RightButton = (1 << 1);

  // We only pay attention to buttons 1 and 2
  const auto newButtons = ev.mButtons & (LeftButton | RightButton);

  if (newButtons & LeftButton) {
    cme.modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  }
  if (newButtons & RightButton) {
    cme.modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
  }

  if (mCursorButtons == newButtons) {
    host->SendMouseMoveEvent(cme, /* mouseLeave = */ false);
    return;
  }

  const auto down = newButtons & ~mCursorButtons;
  const auto up = mCursorButtons & ~newButtons;
  using enum cef_mouse_button_type_t;
  if (down & LeftButton) {
    host->SendMouseClickEvent(cme, MBT_LEFT, /* up = */ false, 1);
  }
  if (up & LeftButton) {
    host->SendMouseClickEvent(cme, MBT_LEFT, /* up = */ true, 1);
  }
  if (down & RightButton) {
    host->SendMouseClickEvent(cme, MBT_RIGHT, /* up = */ false, 1);
  }
  if (up & RightButton) {
    host->SendMouseClickEvent(cme, MBT_RIGHT, /* up = */ true, 1);
  }
  mCursorButtons = newButtons;
}

task<JSAPIResult> ChromiumPageSource::Client::SetPreferredPixelSize(
  uint32_t width,
  uint32_t height) {
  if (width < 1 || height < 1) {
    co_return jsapi_error("Requested 0px area, ignoring");
  }

  PixelSize size {width, height};
  if (
    width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
    || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
    dprint.Warning(
      "Web page requested resize to {}x{}, which is outside of D3D11 limits",
      width,
      height);
    size = size.ScaledToFit(
      {
        D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
        D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
      },
      Geometry2D::ScaleToFitMode::ShrinkOnly);
    if (size.mWidth < 1 || size.mHeight < 1) {
      co_return jsapi_error(
        "Requested size scales down to < 1px in at least 1 dimension");
    }
    dprint("Shrunk to fit: {}x{}", size.mWidth, size.mHeight);
  }

  this->GetRenderHandlerSubclass()->SetSize(size);
  this->GetBrowser()->GetHost()->WasResized();

  co_return nlohmann::json {
    {"result", "resized"},
    {
      "details",
      {
        {"width", size.mWidth},
        {"height", size.mHeight},
      },
    },
  };
}

task<JSAPIResult> ChromiumPageSource::Client::EnableExperimentalFeatures(
  std::vector<ExperimentalFeature> toEnable) {
  std::vector<ExperimentalFeature> enabledThisCall;

  const auto EnableFeature = [&, this](const ExperimentalFeature& feature) {
    dprint.Warning("JS enabled experimental feature {}", feature);
    mEnabledExperimentalFeatures.push_back(feature);
    enabledThisCall.push_back(feature);
  };

  for (auto&& feature: toEnable) {
    if (std::ranges::contains(
          mEnabledExperimentalFeatures,
          feature.mName,
          &ExperimentalFeature::mName)) {
      co_return jsapi_error(
        "Experimental feature `{}` is already enabled", feature.mName);
    }

    if (!std::ranges::contains(SupportedExperimentalFeatures, feature)) {
      co_return jsapi_error(
        "`{}` is not a recognized experimental feature", feature);
    }

    EnableFeature(feature);

    if (feature == GraphicsTabletInfoFeature) {
      this->EnableJSAPI(GraphicsTabletInfoFeature.mName);
    }

    if (feature == SetCursorEventsModeFeature) {
      this->EnableJSAPI(SetCursorEventsModeFeature.mName);
    }

    if (feature == PageBasedContentFeature) {
      this->EnableJSAPI(PageBasedContentFeature.mName);
    }

    if (feature == PageBasedContentWithRequestPageChangeFeature) {
      EnableFeature(PageBasedContentFeature);
      this->EnableJSAPI("PageBasedContentWithRequestPageChange");
    }
  }

  co_return nlohmann::json {
    {"result", std::format("Enabled {} features", enabledThisCall.size())},
    {
      "details",
      {
        {"features", enabledThisCall},
      },
    },
  };
}

task<JSAPIResult> ChromiumPageSource::Client::OpenDeveloperToolsWindow() {
  CefWindowInfo windowInfo;
  windowInfo.windowless_rendering_enabled = false;
  CefBrowserSettings settings;
  mBrowser->GetHost()->ShowDevTools(windowInfo, nullptr, settings, {});
  co_return nlohmann::json {};
}

PageID ChromiumPageSource::Client::GetCurrentPage() const {
  return mCurrentPage;
}

nlohmann::json ChromiumPageSource::Client::GetSupportedExperimentalFeatures() {
  return SupportedExperimentalFeatures;
}

ChromiumPageSource::Client::CursorEventsMode
ChromiumPageSource::Client::GetCursorEventsMode() const {
  return mCursorEventsMode;
}

task<JSAPIResult> ChromiumPageSource::Client::SetCursorEventsMode(
  CursorEventsMode mode) {
  if (!std::ranges::contains(
        mEnabledExperimentalFeatures, SetCursorEventsModeFeature)) {
    co_return jsapi_missing_feature_error(SetCursorEventsModeFeature);
  }
  using enum CursorEventsMode;
  if (
    mode == DoodlesOnly
    && !std::ranges::contains(
      mEnabledExperimentalFeatures, DoodlesOnlyFeature)) {
    co_return jsapi_missing_feature_error(DoodlesOnlyFeature);
  }
  mCursorEventsMode = mode;
  co_return nlohmann::json {};
}

task<JSAPIResult> ChromiumPageSource::Client::GetPages() {
  if (!std::ranges::contains(
        mEnabledExperimentalFeatures, PageBasedContentFeature)) {
    co_return jsapi_missing_feature_error(PageBasedContentFeature);
  }
  const auto pageSource = mPageSource.lock();
  if (!pageSource) {
    co_return jsapi_error("Tab no longer exists");
  }

  {
    std::shared_lock lock(pageSource->mStateMutex);
    if (auto state = get_if<PageBasedState>(&pageSource->mState)) {
      if (state->mPages.empty()) {
        co_return nlohmann::json {{"havePages", false}};
      }
      co_return nlohmann::json {{"havePages", true}, {"pages", state->mPages}};
    }
  }

  std::unique_lock lock(pageSource->mStateMutex);
  if (auto state = get_if<PageBasedState>(&pageSource->mState)) {
    if (state->mPages.empty()) {
      co_return nlohmann::json {{"havePages", false}};
    }
    co_return nlohmann::json {{"havePages", true}, {"pages", state->mPages}};
  }
  auto primaryClient = get<ScrollableState>(pageSource->mState).mClient;
  OPENKNEEBOARD_ASSERT(primaryClient->GetBrowserID() == this->GetBrowserID());
  pageSource->mState = PageBasedState {primaryClient};
  co_return nlohmann::json {{"havePages", false}};
}

task<JSAPIResult> ChromiumPageSource::Client::SetPages(
  std::vector<APIPage> pages) {
  auto pageSource = mPageSource.lock();
  if (!pageSource) {
    co_return jsapi_error("Tab no longer exists");
  }
  {
    std::unique_lock lock(pageSource->mStateMutex);
    auto state = get_if<PageBasedState>(&pageSource->mState);
    if (!state) {
      co_return jsapi_error(
        "SetPages() called without first calling GetPages()");
    }

    state->mPages = pages;
  }

  const auto messageBody = 
    nlohmann::json {
      {"pages", pages},
    }.dump();

  {
    std::shared_lock lock(pageSource->mStateMutex);
    for (auto&& [id, client]:
         get<PageBasedState>(pageSource->mState).mClients) {
      if (client->GetBrowserID() == this->GetBrowserID()) {
        continue;
      }

      auto message = CefProcessMessage::Create("okbEvent/apiEvent");
      auto args = message->GetArgumentList();
      args->SetString(0, "pagesChanged");
      args->SetString(1, messageBody);
      client->mBrowser->GetMainFrame()->SendProcessMessage(
        PID_RENDERER, message);
    }
  }

  pageSource->evContentChangedEvent.EnqueueForContext(mUIThread);
  pageSource->evAvailableFeaturesChangedEvent.EnqueueForContext(mUIThread);

  co_return nlohmann::json {};
}

task<JSAPIResult> ChromiumPageSource::Client::RequestPageChange(
  nlohmann::json data) {
  const auto pageSource = mPageSource.lock();
  if (!pageSource) {
    co_return jsapi_error("Tab no longer exists.");
  }

  std::shared_lock lock(pageSource->mStateMutex);
  auto state = get_if<PageBasedState>(&pageSource->mState);
  if (!state) {
    co_return jsapi_error(
      "RequestPageChange() called without calling GetPages() first");
  }

  if (
    pageSource->mKind != Kind::Plugin
    && (std::chrono::steady_clock::now() - mLastCursorEventAt)
      > std::chrono::milliseconds(100)) {
    co_return jsapi_error(
      "Web Dashboards can only call `RequestPageChange()` shortly after a "
      "cursor event; to remove this limit, create an OpenKneeboard plugin.");
  }

  const auto guid = data.at("guid").get<winrt::guid>();
  auto pageIt = std::ranges::find(state->mPages, guid, &APIPage::mGuid);
  if (pageIt == state->mPages.end()) {
    co_return jsapi_error("Couldn't find page with GUID {}", guid);
  }
  const auto page = *pageIt;
  const auto clientIt = std::ranges::find(
    state->mClients, this, [](const auto& pair) { return pair.second.get(); });
  if (clientIt == state->mClients.end()) {
    co_return jsapi_error("Couldn't find kneeboardViewID for current client");
  }
  const auto viewID = clientIt->first;
  lock.unlock();

  this->SetCurrentPage(page.mPageID, page.mPixelSize);
}

void ChromiumPageSource::Client::SetCurrentPage(
  PageID pageID,
  const PixelSize& size) {
  if (pageID == mCurrentPage) {
    return;
  }

  if (size != mRenderHandler->GetSize()) {
    mRenderHandler->SetSize(size);
    mBrowser->GetHost()->WasResized();
  }

  mCurrentPage = pageID;

  if (!mViewID.has_value()) {
    return;
  }

  const auto pageSource = mPageSource.lock();
  if (!pageSource) {
    return;
  }

  std::shared_lock lock(pageSource->mStateMutex);
  auto state = get_if<PageBasedState>(&pageSource->mState);
  if (!state) {
    return;
  }
  auto it = std::ranges::find(state->mPages, pageID, &APIPage::mPageID);
  if (it == state->mPages.end()) {
    return;
  }
  auto page = *it;
  lock.unlock();

  auto message = CefProcessMessage::Create("okbEvent/apiEvent");
  auto args = message->GetArgumentList();
  args->SetString(0, "pageChanged");
  args->SetString(1, nlohmann::json {{"page", page}}.dump());

  mBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);

  pageSource->evPageChangeRequestedEvent.EnqueueForContext(
    mUIThread, mViewID.value(), page.mPageID);
}

task<JSAPIResult> ChromiumPageSource::Client::SendMessageToPeers(
  nlohmann::json apiMessage) {
  auto pageSource = mPageSource.lock();
  if (!pageSource) {
    co_return jsapi_error("Tab no longer exists");
  }
  std::shared_lock lock(pageSource->mStateMutex);
  auto state = get_if<PageBasedState>(&pageSource->mState);
  if (!state) {
    co_return jsapi_error(
      "SendMessageToPeers() called without first calling GetPages()");
  }

  const auto messageBody = nlohmann::json {{"message", apiMessage}}.dump();

  const auto myID = mBrowser->GetIdentifier();
  for (auto&& [view, client]: state->mClients) {
    auto browser = client->GetBrowser();
    if (!browser) {
      continue;
    }
    if (browser->GetIdentifier() == myID) {
      continue;
    }
    auto message = CefProcessMessage::Create("okbEvent/apiEvent");
    auto args = message->GetArgumentList();
    args->SetString(0, "peerMessage");
    args->SetString(1, messageBody);
    OPENKNEEBOARD_ASSERT(message->IsValid());
    browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
  }
  co_return {};
}

void ChromiumPageSource::Client::SetViewID(KneeboardViewID id) {
  mViewID = id;
}

void ChromiumPageSource::Client::PostCustomAction(
  std::string_view actionID,
  const nlohmann::json& arg) {
  auto message = CefProcessMessage::Create("okbEvent/apiEvent");
  auto args = message->GetArgumentList();
  args->SetString(0, "plugin/tab/customAction");
  args->SetString(
    1,
    nlohmann::json {
      {"id", actionID},
      {"extraData", arg},
    }
      .dump());
  mBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
}

task<JSAPIResult> ChromiumPageSource::Client::GetGraphicsTabletInfo() {
  auto pageSource = mPageSource.lock();
  if (!pageSource) {
    co_return jsapi_error("Tab no longer exists");
  }

  auto tablets = pageSource->mKneeboard->GetTabletInputAdapter();
  std::optional<TabletInfo> tablet;
  if (tablets) {
    const auto info = tablets->GetTabletInfo();
    if (!info.empty()) {
      tablet = {info.front()};
    }
  }
  auto orientation = TabletSettings::Device {}.mOrientation;
  if (tablet) {
    const auto& devices = tablets->GetSettings().mDevices;
    if (devices.contains(tablet->mDeviceID)) {
      orientation = devices.at(tablet->mDeviceID).mOrientation;
    }
  }

  if (!tablet) {
    co_return nlohmann::json {
      {"HaveTablet", false},
      {"SuggestedPixelSize", Config::DefaultPixelSize},
    };
  }

  const auto inputResolution = Geometry2D::Size {
    tablet->mMaxX,
    tablet->mMaxY,
  };
  const auto gcd = std::gcd<uint32_t, uint32_t>(tablet->mMaxX, tablet->mMaxY);
  auto suggested = Geometry2D::Size {
    tablet->mMaxX / gcd,
    tablet->mMaxY / gcd,
  }.Rounded<uint32_t>().IntegerScaledToFit(
    {1024, 1024}, Geometry2D::ScaleToFitMode::ShrinkOrGrow);

  using TO = TabletOrientation;
  if (orientation == TO::RotateCW270 || orientation == TO::RotateCW90) {
    std::swap(suggested.mWidth, suggested.mHeight);
  }

  co_return nlohmann::json {
    {"HaveTablet", true},
    {"InputResolution", inputResolution},
    {"InputOrientation", orientation},
    {"SuggestedPixelSize", suggested},
  };
}

}// namespace OpenKneeboard