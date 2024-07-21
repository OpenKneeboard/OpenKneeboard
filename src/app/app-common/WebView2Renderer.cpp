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

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/Version.h>
#include <OpenKneeboard/WebView2Renderer.h>

#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/json_format.h>

#include <shims/winrt/base.h>

#include <format>
#include <fstream>

namespace OpenKneeboard {

void to_json(nlohmann::json& j, const WebView2Renderer::APIPage& v) {
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

void from_json(const nlohmann::json& j, WebView2Renderer::APIPage& v) {
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

#define IT(x) {CursorTouchState::x, #x},
NLOHMANN_JSON_SERIALIZE_ENUM(
  CursorTouchState,
  {OPENKNEEBOARD_CURSORTOUCHSTATE_VALUES});
#undef IT

namespace {
const ExperimentalFeature RawCursorEventsFeature {
  "RawCursorEvents",
  2024071801};
const ExperimentalFeature RawCursorEventsToggleableFeature {
  "RawCursorEvents",
  2024071802};
const ExperimentalFeature DoodlesOnlyFeature {"DoodlesOnly", 2024071801};
const ExperimentalFeature DoodlesOnlyToggleableFeature {
  "DoodlesOnly",
  2024071802};

const ExperimentalFeature SetCursorEventsModeFeature {
  "SetCursorEventsMode",
  2024071801};

const ExperimentalFeature PageBasedContentFeature {
  "PageBasedContent",
  2024072001};

std::array SupportedExperimentalFeatures {
  RawCursorEventsFeature,
  RawCursorEventsToggleableFeature,
  DoodlesOnlyFeature,
  DoodlesOnlyToggleableFeature,
  SetCursorEventsModeFeature,
  PageBasedContentFeature,
};

template <class... Args>
auto jsapi_error(std::format_string<Args...> fmt, Args&&... args) {
  return std::unexpected {std::format(fmt, std::forward<Args>(args)...)};
}

};// namespace

OPENKNEEBOARD_DEFINE_JSON(ExperimentalFeature, mName, mVersion);

WebView2Renderer::~WebView2Renderer() = default;

static wchar_t WindowClassName[] {L"OpenKneeboard/WebView2Host"};

void WebView2Renderer::RegisterWindowClass() {
  WNDCLASSW windowClass {
    .style = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = &WebView2Renderer::WindowProc,
    .hInstance = GetModuleHandle(nullptr),
    .lpszClassName = WindowClassName,
  };
  ::RegisterClassW(&windowClass);
}

void WebView2Renderer::CreateBrowserWindow() {
  OPENKNEEBOARD_TraceLoggingScope("WebView2Renderer::CreateBrowserWindow()");

  {
    static std::once_flag sFlag;
    std::call_once(sFlag, &WebView2Renderer::RegisterWindowClass);
  }

  mBrowserWindow = unique_hwnd {CreateWindowExW(
    WS_EX_NOACTIVATE,
    WindowClassName,
    L"OpenKneeboard WebView2 Host",
    0,
    CW_DEFAULT,
    CW_DEFAULT,
    0,
    0,
    HWND_MESSAGE,
    NULL,
    GetModuleHandle(nullptr),
    nullptr)};
  if (!mBrowserWindow) {
    const auto code = GetLastError();
    auto e = std::system_category().default_error_condition(code);
    dprintf(
      "Failed to create browser window: {:#x} - {}",
      std::bit_cast<uint32_t>(code),
      e.message());
    OPENKNEEBOARD_BREAK;
  }
}

winrt::Windows::Foundation::IAsyncAction
WebView2Renderer::InitializeContentToCapture() {
  TraceLoggingActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "WebView2Renderer::InitializeContentToCapture");
  co_await winrt::resume_foreground(mDQC.DispatcherQueue());

  mWorkerThread = {};
  SetThreadDescription(GetCurrentThread(), L"OKB WebView2 Worker");

  this->CreateBrowserWindow();

  this->InitializeComposition();

  using namespace winrt::Microsoft::Web::WebView2::Core;

  const auto windowRef
    = CoreWebView2ControllerWindowReference::CreateFromWindowHandle(
      reinterpret_cast<uint64_t>(mBrowserWindow.get()));

  mController
    = co_await mEnvironment.CreateCoreWebView2CompositionControllerAsync(
      windowRef);

  if (mSettings.mTransparentBackground) {
    mController.DefaultBackgroundColor(
      winrt::Windows::UI::Colors::Transparent());
  }

  mWebView = mController.CoreWebView2();

  auto settings = mWebView.Settings();
  const auto userAgent = std::format(
    L"{} OpenKneeboard/{}.{}.{}.{}",
    std::wstring_view {settings.UserAgent()},
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build);
  settings.UserAgent(userAgent);

  settings.IsWebMessageEnabled(true);
  mWebView.WebMessageReceived(
    std::bind_front(&WebView2Renderer::OnWebMessageReceived, this));
  mWebView.NavigationStarting(std::bind_front(
    [](auto weak, auto thread, auto, auto args) -> winrt::fire_and_forget {
      co_await thread;
      auto self = std::static_pointer_cast<WebView2Renderer>(weak.lock());
      if (!self) {
        co_return;
      }
      self->mDocumentResources = {};
      const auto originalURI = self->mSettings.mURI;
      const auto newURI = winrt::to_string(args.Uri());
      if (originalURI == newURI) {
        self->mDocumentResources.mPages = self->mInitialPages;
      }
    },
    weak_from_this(),
    mUIThread));

  co_await this->ImportJavascriptFile(
    Filesystem::GetImmutableDataDirectory() / "WebView2.js");

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
    {"AvailableExperimentalFeatures", SupportedExperimentalFeatures},
  };

  if (mViewInfo) {
    initData.emplace(
      "PeerInfo",
      nlohmann::json {
        {"ThisInstance",
         {
           {"ViewGUID", std::format("{:nobraces}", mViewInfo->mGuid)},
           {"ViewName", mViewInfo->mName},
         }},
      });
  }

  co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
    std::format(L"window.OpenKneeboard = new OpenKneeboardAPI({});", initData));

  if (mSettings.mIntegrateWithSimHub) {
    co_await this->ImportJavascriptFile(
      Filesystem::GetImmutableDataDirectory() / "WebView2-SimHub.js");
  }

  mController.BoundsMode(CoreWebView2BoundsMode::UseRawPixels);
  mController.RasterizationScale(1.0);
  mController.ShouldDetectMonitorScaleChanges(false);
  mController.Bounds({
    0,
    0,
    mSize.Width<float>(),
    mSize.Height<float>(),
  });

  mController.RootVisualTarget(mWebViewVisual);
  mController.IsVisible(true);

  if (mSettings.mOpenDeveloperToolsWindow) {
    mWebView.OpenDevToolsWindow();
  }

  mWebView.Navigate(winrt::to_hstring(mSettings.mURI));

  TraceLoggingWriteStop(
    activity,
    "WebView2Renderer::InitializeContentToCapture",
    TraceLoggingValue("Finished", "Result"));
}

std::optional<float> WebView2Renderer::GetHDRWhiteLevelInNits() const {
  return {};
}

winrt::Windows::Graphics::DirectX::DirectXPixelFormat
WebView2Renderer::GetPixelFormat() const {
  return winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
    B8G8R8A8UIntNormalized;
}

winrt::Windows::Graphics::Capture::GraphicsCaptureItem
WebView2Renderer::CreateWGCaptureItem() {
  return winrt::Windows::Graphics::Capture::GraphicsCaptureItem::
    CreateFromVisual(mRootVisual);
}

PixelRect WebView2Renderer::GetContentRect(const PixelSize& captureSize) const {
  return {{0, 0}, mSize};
}

PixelSize WebView2Renderer::GetSwapchainDimensions(
  const PixelSize& captureSize) const {
  return captureSize;
}

void WebView2Renderer::PostFrame() {
  this->FlushCursorEvents();
}

void WebView2Renderer::OnJSAPI_Peer_SendMessageToPeers(
  const WebView2Renderer::InstanceID& sender,
  const nlohmann::json& message) {
  if (!mViewInfo) {
    return;
  }

  if (mViewInfo->mGuid == sender) {
    return;
  }

  if (mDocumentResources.mContentMode != ContentMode::PageBased) {
    // No 'peers'
    return;
  }

  this->SendJSEvent(
    "peerMessage",
    {{
      "detail",
      {
        {"message", message},
      },
    }});
}

void WebView2Renderer::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& event) {
  if (!mController) {
    return;
  }

  switch (mDocumentResources.mCursorEventsMode) {
    case CursorEventsMode::MouseEmulation: {
      std::unique_lock lock(mCursorEventsMutex);
      mCursorEvents.push(event);
      return;
    }
    case CursorEventsMode::Raw:
      this->SendJSEvent(
        "cursor",
        {
          {"detail",
           {
             {"touchState", event.mTouchState},
             {"buttons", event.mButtons},
             {"position",
              {
                {"x", event.mX},
                {"y", event.mY},
              }},
           }},
        });
      return;
    case CursorEventsMode::DoodlesOnly: {
      mDoodles->PostCursorEvent(
        ctx, event, mDocumentResources.mCurrentPage, mSize);
      return;
    }
  }
}

winrt::fire_and_forget WebView2Renderer::SendJSEvent(
  std::string_view eventType,
  nlohmann::json eventOptions) {
  auto weak = weak_from_this();
  auto thread = mWorkerThread;
  co_await thread;
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  const nlohmann::json message {
    {"OpenKneeboard_WebView2_MessageType", "Event"},
    {"eventType", eventType},
    {"eventOptions", eventOptions},
  };

  mWebView.PostWebMessageAsJson(winrt::to_hstring(message.dump()));
}

winrt::fire_and_forget WebView2Renderer::ActivateJSAPI(std::string_view api) {
  auto weak = weak_from_this();
  auto thread = mWorkerThread;
  co_await thread;
  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  const nlohmann::json message {
    {"OpenKneeboard_WebView2_MessageType", "ActivateAPI"},
    {"api", api},
  };

  mWebView.PostWebMessageAsJson(winrt::to_hstring(message.dump()));
}

winrt::Windows::Foundation::IAsyncAction WebView2Renderer::ImportJavascriptFile(
  std::filesystem::path path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  const auto js = ss.str();

  co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
    winrt::to_hstring(js));
}

void WebView2Renderer::RenderPage(
  const RenderContext& rc,
  PageID page,
  const PixelRect& rect) {
  auto& dr = mDocumentResources;
  if (dr.mContentMode == ContentMode::PageBased && dr.mCurrentPage != page) {
    auto it = std::ranges::find(dr.mPages, page, &APIPage::mPageID);
    if (it != dr.mPages.end()) {
      dr.mCurrentPage = page;
      this->Resize(it->mPixelSize);
      this->SendJSEvent("pageChanged", {{"detail", {{"page", *it}}}});
    }
  }

  auto rt = rc.GetRenderTarget();

  WGCRenderer::Render(rt, rect);

  if (
    dr.mContentMode == ContentMode::PageBased
    || dr.mCursorEventsMode == CursorEventsMode::DoodlesOnly) {
    mDoodles->Render(rt, page, rect);
  }
}

winrt::fire_and_forget WebView2Renderer::FlushCursorEvents() {
  if (!mWorkerThread) {
    co_return;
  }
  if (mCursorEvents.empty()) {
    co_return;
  }
  auto weakThis = weak_from_this();
  co_await mWorkerThread;
  auto self = weakThis.lock();
  if (!self) {
    co_return;
  }

  std::queue<CursorEvent> events;
  {
    std::unique_lock lock(mCursorEventsMutex);
    events = std::move(mCursorEvents);
    mCursorEvents = {};
  }

  while (!events.empty()) {
    const auto event = events.front();
    events.pop();

    // TODO: button tracking
    using namespace winrt::Microsoft::Web::WebView2::Core;

    using EVKind = CoreWebView2MouseEventKind;
    using VKey = CoreWebView2MouseEventVirtualKeys;

    VKey keys {};
    if ((event.mButtons & 1)) {
      keys |= VKey::LeftButton;
    }
    if ((event.mButtons & (1 << 1))) {
      keys |= VKey::RightButton;
    }

    if (event.mTouchState == CursorTouchState::NotNearSurface) {
      if (mMouseButtons & 1) {
        mController.SendMouseInput(EVKind::LeftButtonUp, keys, 0, {});
      }
      if (mMouseButtons & (1 << 1)) {
        mController.SendMouseInput(EVKind::RightButtonUp, keys, 0, {});
      }
      mMouseButtons = {};
      mController.SendMouseInput(EVKind::Leave, keys, 0, {});
      continue;
    }

    // We only pay attention to left/right buttons
    const auto buttons = event.mButtons & 3;
    if (buttons == mMouseButtons) {
      mController.SendMouseInput(EVKind::Move, keys, 0, {event.mX, event.mY});
      continue;
    }

    const auto down = event.mButtons & ~mMouseButtons;
    const auto up = mMouseButtons & ~event.mButtons;
    mMouseButtons = buttons;

    if (down & 1) {
      mController.SendMouseInput(
        EVKind::LeftButtonDown, keys, 0, {event.mX, event.mY});
    }
    if (up & 1) {
      mController.SendMouseInput(
        EVKind::LeftButtonUp, keys, 0, {event.mX, event.mY});
    }
    if (down & (1 << 1)) {
      mController.SendMouseInput(
        EVKind::RightButtonDown, keys, 0, {event.mX, event.mY});
    }
    if (up & (1 << 1)) {
      mController.SendMouseInput(
        EVKind::RightButtonUp, keys, 0, {event.mX, event.mY});
    }
  }
}

winrt::fire_and_forget WebView2Renderer::OnWebMessageReceived(
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2,
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2WebMessageReceivedEventArgs
    args) {
  const auto weak = weak_from_this();

  const auto json = to_string(args.WebMessageAsJson());
  const auto parsed = nlohmann::json::parse(json);

  if (!parsed.contains("messageName")) {
    co_return;
  }

  const std::string message = parsed.at("messageName");
  const uint64_t callID = parsed.at("callID");

  const auto respond = std::bind_front(
    [](auto thread, auto weak, auto callID, OKBPromiseResult result)
      -> winrt::fire_and_forget {
      co_await thread;
      auto self = std::static_pointer_cast<WebView2Renderer>(weak.lock());
      if (!self) {
        co_return;
      }
      nlohmann::json response {
        {"OpenKneeboard_WebView2_MessageType", "AsyncResponse"},
        {"callID", callID},
      };
      if (result.has_value()) {
        if (result->is_null()) {
          response.emplace("result", "ok");
        } else {
          response.emplace("result", result.value());
        }
      } else {
        response.emplace("error", result.error());
        dprintf("WARNING: WebView2 API error: {}", result.error());
      }

      self->mWebView.PostWebMessageAsJson(winrt::to_hstring(response.dump()));
    },
    mWorkerThread,
    weak,
    callID);

#define OKB_INVOKE_JSAPI(APIFUNC) \
  if (message == "OpenKneeboard." #APIFUNC) { \
    respond(co_await this->JSAPI_##APIFUNC(parsed.at("messageData"))); \
    co_return; \
  }
  OPENKNEEBOARD_JSAPI_METHODS(OKB_INVOKE_JSAPI)
#undef OKB_CALL_JSAPI

  OPENKNEEBOARD_BREAK;
  respond(jsapi_error("Invalid JS API request: {}", message));
}

concurrency::task<WebView2Renderer::OKBPromiseResult>
WebView2Renderer::JSAPI_SetPreferredPixelSize(nlohmann::json args) {
  PixelSize size = {
    args.at("width"),
    args.at("height"),
  };
  if (size.mWidth < 1 || size.mHeight < 1) {
    co_return jsapi_error("WebView2 requested 0px area, ignoring");
  }
  if (
    size.mWidth > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
    || size.mHeight > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
    dprintf(
      "WebView2 requested size {}x{} is outside of D3D11 limits",
      size.mWidth,
      size.mHeight);
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
    dprintf("Shrunk to fit: {}x{}", size.mWidth, size.mHeight);
  }

  const auto success = [&size](auto result) {
    return nlohmann::json {
      {"result", result},
      {
        "details",
        {
          {"width", size.mWidth},
          {"height", size.mHeight},
        },
      },
    };
  };

  if (mSize == size) {
    co_return success("no change");
  }

  co_await this->Resize(size);
  co_return success("resized");
}

winrt::Windows::Foundation::IAsyncAction WebView2Renderer::Resize(
  PixelSize size) {
  if (mSize == size) {
    co_return;
  }
  auto weak = weak_from_this();
  const auto uiThread = mUIThread;

  mSize = size;

  const auto wfSize
    = mSize.StaticCast<float, winrt::Windows::Foundation::Numerics::float2>();
  mRootVisual.Size(wfSize);
  mWebViewVisual.Size(wfSize);
  mController.Bounds({0, 0, mSize.Width<float>(), mSize.Height<float>()});
  mController.RasterizationScale(1.0);

  co_await uiThread;

  auto self = weak.lock();
  if (!self) {
    co_return;
  }
  WGCRenderer::ForceResize(size);

  evNeedsRepaintEvent.Emit();
}

concurrency::task<WebView2Renderer::OKBPromiseResult>
WebView2Renderer::JSAPI_SetCursorEventsMode(nlohmann::json args) {
  auto& pageApi = mDocumentResources;

  auto missingFeature = [](auto feature) {
    return jsapi_error(
      "SetCursorEventMode() failed - the experimental feature `{}` "
      "version "
      "`{}` is required.",
      feature.mName,
      feature.mVersion);
  };
  if (!std::ranges::contains(
        pageApi.mEnabledExperimentalFeatures, SetCursorEventsModeFeature)) {
    co_return missingFeature(SetCursorEventsModeFeature);
  }
  const std::string mode = args.at("mode");

  auto success = []() { return nlohmann::json {{"result", "success"}}; };

  if (mode == "MouseEmulation") {
    pageApi.mCursorEventsMode = CursorEventsMode::MouseEmulation;
    co_return success();
  }

  if (mode == "DoodlesOnly") {
    if (!std::ranges::contains(
          pageApi.mEnabledExperimentalFeatures, DoodlesOnlyToggleableFeature)) {
      co_return missingFeature(DoodlesOnlyToggleableFeature);
    }
    pageApi.mCursorEventsMode = CursorEventsMode::DoodlesOnly;
    co_return success();
  }

  if (mode == "Raw") {
    if (!std::ranges::contains(
          pageApi.mEnabledExperimentalFeatures,
          RawCursorEventsToggleableFeature)) {
      co_return missingFeature(RawCursorEventsFeature);
    }
    pageApi.mCursorEventsMode = CursorEventsMode::Raw;
    co_return success();
  }

  co_return jsapi_error("Unrecognized mode '{}'", mode);
}

concurrency::task<WebView2Renderer::OKBPromiseResult>
WebView2Renderer::JSAPI_GetPages(nlohmann::json args) {
  auto& dr = mDocumentResources;
  if (!std::ranges::contains(
        dr.mEnabledExperimentalFeatures, PageBasedContentFeature)) {
    co_return jsapi_error(
      "The experimental feature {} is required.", PageBasedContentFeature);
  }

  if ((!dr.mPages.empty()) && dr.mContentMode == ContentMode::Scrollable) {
    dr.mContentMode = ContentMode::PageBased;
  }

  co_return nlohmann::json {
    {"havePages", !dr.mPages.empty()},
    {"pages", dr.mPages},
  };
}

concurrency::task<WebView2Renderer::OKBPromiseResult>
WebView2Renderer::JSAPI_SendMessageToPeers(nlohmann::json args) {
  auto& dr = mDocumentResources;

  if (!std::ranges::contains(
        dr.mEnabledExperimentalFeatures, PageBasedContentFeature)) {
    co_return jsapi_error(
      "The experimental feature {} is required.", PageBasedContentFeature);
  }

  if (!mViewInfo) {
    co_return jsapi_error("Pages have not been set; no peers exist");
  }

  evJSAPI_SendMessageToPeers.Emit(mViewInfo->mGuid, args.at("message"));

  co_return nlohmann::json {};
}

concurrency::task<WebView2Renderer::OKBPromiseResult>
WebView2Renderer::JSAPI_SetPages(nlohmann::json args) {
  auto weak = weak_from_this();
  auto thread = mUIThread;

  auto& pageApi = mDocumentResources;
  if (!std::ranges::contains(
        pageApi.mEnabledExperimentalFeatures, PageBasedContentFeature)) {
    co_return jsapi_error(
      "The experimental feature {} is required.", PageBasedContentFeature);
  }

  std::vector<APIPage> pages;
  for (const auto& it: args.at("pages")) {
    auto page = it.get<APIPage>();
    const auto old
      = std::ranges::find(pageApi.mPages, page.mGuid, &APIPage::mGuid);
    if (old != pageApi.mPages.end()) {
      // Use the new pixelsize and data, but keep existing internal ID
      page.mPageID = old->mPageID;
    }

    if (page.mPixelSize.IsEmpty()) {
      page.mPixelSize = mSize;
    }
    pages.push_back(page);
  }

  if (pages.empty()) {
    co_return jsapi_error("Must provide at least one page definition");
  }

  pageApi.mPages = std::move(pages);
  pageApi.mContentMode = ContentMode::PageBased;

  co_await thread;
  auto self = weak.lock();
  if (!self) {
    co_return jsapi_error("WebView2 lifetime exceeded.");
  }
  evJSAPI_SetPages.Emit(pageApi.mPages);

  co_return nlohmann::json {};
}

concurrency::task<WebView2Renderer::OKBPromiseResult>
WebView2Renderer::JSAPI_EnableExperimentalFeatures(nlohmann::json args) {
  std::vector<ExperimentalFeature> enabledFeatures;

  auto& pageApi = mDocumentResources;

  for (const auto& featureSpec: args.at("features")) {
    const std::string name = featureSpec.at("name");
    const uint64_t version = featureSpec.at("version");

    if (std::ranges::contains(
          pageApi.mEnabledExperimentalFeatures,
          name,
          &ExperimentalFeature::mName)) {
      co_return jsapi_error(
        "Experimental feature `{}` is already enabled", name);
    }

    const ExperimentalFeature feature {name, version};

    if (!std::ranges::contains(SupportedExperimentalFeatures, feature)) {
      if (!std::ranges::contains(
            SupportedExperimentalFeatures, name, &ExperimentalFeature::mName)) {
        co_return jsapi_error(
          "`{}` is not a recognized experimental feature", name);
      }

      co_return jsapi_error(
        "`{}` is a recognized experimental feature, but `{}` is not a "
        "supported version",
        name,
        version);
    }

    dprintf(
      "WARNING: JS enabled experimental feature `{}` version `{}`",
      name,
      version);

    pageApi.mEnabledExperimentalFeatures.push_back(feature);
    enabledFeatures.push_back(feature);

    if (
      feature == RawCursorEventsToggleableFeature
      || feature == DoodlesOnlyToggleableFeature) {
      // Nothing to do except enable these
      continue;
    }

    if (feature == SetCursorEventsModeFeature) {
      this->ActivateJSAPI("SetCursorEventsMode");
      continue;
    }

    if (feature == PageBasedContentFeature) {
      this->ActivateJSAPI("PageBasedContent");
      continue;
    }

    auto warnObsolete = [this](const auto& feature) {
      const auto warning = std::format(
        "WARNING: enabling an obsolete experimental feature: `{}` "
        "version "
        "`{}`",
        feature.mName,
        feature.mVersion);
      dprint(warning);
      this->SendJSLog(warning);
    };

    if (feature == RawCursorEventsFeature) {
      warnObsolete(feature);
      if (pageApi.mCursorEventsMode != CursorEventsMode::MouseEmulation) {
        co_return jsapi_error(
          "Can not enable `{}`, as the cursor mode has already been "
          "changed "
          "by "
          "this page.",
          name);
      }
      pageApi.mCursorEventsMode = CursorEventsMode::Raw;
      continue;
    }

    if (feature == DoodlesOnlyFeature) {
      warnObsolete(feature);
      if (pageApi.mCursorEventsMode != CursorEventsMode::MouseEmulation) {
        co_return jsapi_error(
          "Can not enable `{}`, as the cursor mode has already been "
          "changed by this page.",
          name);
      }
      pageApi.mCursorEventsMode = CursorEventsMode::DoodlesOnly;
      continue;
    }

    const auto message = std::format(
      "OpenKneeboard internal error: `{}` v{} is a recognized but "
      "unhandled experimental feature",
      name,
      version);
    dprint(message);
    OPENKNEEBOARD_BREAK;
    co_return jsapi_error("{}", message);
  }

  co_return nlohmann::json {
    {"result", std::format("enabled {} features", enabledFeatures.size())},
    {"details",
     {
       {"features", enabledFeatures},
     }},
  };
}

winrt::fire_and_forget WebView2Renderer::final_release(
  std::unique_ptr<WebView2Renderer> self) {
  if (self->mWorkerThread) {
    co_await self->mWorkerThread;

    self->mWebView = nullptr;
    self->mController = nullptr;
    self->mEnvironment = nullptr;
    self->mWebViewVisual = nullptr;
    self->mRootVisual = nullptr;
    self->mCompositor = nullptr;
    self->mBrowserWindow.reset();

    self->mWorkerThread = {nullptr};
  }
  co_await self->mUIThread;

  auto wgcSelf = std::unique_ptr<WGCRenderer> {self.release()};
  WGCRenderer::final_release(std::move(wgcSelf));
  co_return;
}

void WebView2Renderer::InitializeComposition() noexcept {
  OPENKNEEBOARD_TraceLoggingScope("WebView2Renderer::InitializeComposition");
  if (mCompositor) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mCompositor = {};
  mRootVisual = mCompositor.CreateContainerVisual();
  mRootVisual.Size(
    mSize.StaticCast<float, winrt::Windows::Foundation::Numerics::float2>());
  mRootVisual.IsVisible(true);

  mWebViewVisual = mCompositor.CreateContainerVisual();
  mRootVisual.Children().InsertAtTop(mWebViewVisual);
  mWebViewVisual.RelativeSizeAdjustment({1, 1});
}

std::shared_ptr<WebView2Renderer> WebView2Renderer::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings,
  const std::shared_ptr<DoodleRenderer>& doodles,
  const winrt::Windows::System::DispatcherQueueController& workerDQC,
  const winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment&
    environment,
  KneeboardView* view,
  const std::vector<APIPage>& apiPages) {
  auto ret = shared_with_final_release(new WebView2Renderer(
    dxr, kbs, settings, doodles, workerDQC, environment, view, apiPages));
  ret->Init();
  return ret;
}

WebView2Renderer::WebView2Renderer(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings,
  const std::shared_ptr<DoodleRenderer>& doodles,
  const winrt::Windows::System::DispatcherQueueController& workerDQC,
  const winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment&
    environment,
  KneeboardView* view,
  const std::vector<APIPage>& apiPages)
  : WGCRenderer(dxr, kbs, {}),
    mDXResources(dxr),
    mKneeboard(kbs),
    mSettings(settings),
    mSize(settings.mInitialSize),
    mDoodles(doodles),
    mDQC(workerDQC),
    mEnvironment(environment),
    mInitialPages(apiPages) {
  if (view) {
    mViewInfo
      = ViewInfo {view->GetPersistentGUID(), std::string {view->GetName()}};
  }
}

void WebView2Renderer::OnJSAPI_Peer_SetPages(
  const std::vector<APIPage>& pages) {
  if (pages == mDocumentResources.mPages) {
    return;
  }

  mDocumentResources.mPages = pages;

  this->SendJSEvent("pagesChanged", {{"detail", {{"pages", pages}}}});
}

LRESULT CALLBACK WebView2Renderer::WindowProc(
  HWND const window,
  UINT const message,
  WPARAM const wparam,
  LPARAM const lparam) noexcept {
  return DefWindowProc(window, message, wparam, lparam);
}

}// namespace OpenKneeboard