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

#include <Windows.h>

#include <fstream>
#include <sstream>
#include <system_error>

#include <WebView2.h>
#include <wrl.h>

namespace OpenKneeboard {

#define IT(x) {CursorTouchState::x, #x},
NLOHMANN_JSON_SERIALIZE_ENUM(
  CursorTouchState,
  {OPENKNEEBOARD_CURSORTOUCHSTATE_VALUES});
#undef IT

using ExFeature = WebView2PageSource::ExperimentalFeature;

namespace {
const ExFeature RawCursorEventsFeature {"RawCursorEvents", 2024071801};
const ExFeature RawCursorEventsToggleableFeature {
  "RawCursorEvents",
  2024071802};
const ExFeature DoodlesOnlyFeature {"DoodlesOnly", 2024071801};
const ExFeature DoodlesOnlyToggleableFeature {"DoodlesOnly", 2024071802};

const ExFeature SetCursorEventsModeFeature {"SetCursorEventsMode", 2024071801};

std::array SupportedExperimentalFeatures {
  RawCursorEventsFeature,
  RawCursorEventsToggleableFeature,
  DoodlesOnlyFeature,
  DoodlesOnlyToggleableFeature,
  SetCursorEventsModeFeature,
};

};// namespace

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ExFeature, mName, mVersion);

std::shared_ptr<WebView2PageSource> WebView2PageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings) {
  auto ret
    = shared_with_final_release(new WebView2PageSource(dxr, kbs, settings));
  ret->Init();
  return ret;
}

WebView2PageSource::WebView2PageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings)
  : WGCPageSource(dxr, kbs, {}),
    mDXResources(dxr),
    mKneeboard(kbs),
    mSettings(settings),
    mSize(mSettings.mInitialSize) {
  OPENKNEEBOARD_TraceLoggingScope("WebView2PageSource::WebView2PageSource()");
  if (!IsAvailable()) {
    return;
  }
  mDQC = winrt::Windows::System::DispatcherQueueController::
    CreateOnDedicatedThread();
}

winrt::Windows::Foundation::IAsyncAction
WebView2PageSource::InitializeContentToCapture() {
  TraceLoggingActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "WebView2PageSource::InitializeContentToCapture");
  if (!IsAvailable()) {
    TraceLoggingWriteStop(
      activity,
      "WebView2PageSource::InitializeContentToCapture",
      TraceLoggingValue("WebView2 is not available", "Result"));
    co_return;
  }

  co_await winrt::resume_foreground(mDQC.DispatcherQueue());

  mWorkerThread = {};
  SetThreadDescription(GetCurrentThread(), L"OKB WebView2 Worker");

  this->CreateBrowserWindow();

  auto dq = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();

  this->InitializeComposition();

  using namespace winrt::Microsoft::Web::WebView2::Core;

  const auto userData = Filesystem::GetLocalAppDataDirectory() / "WebView2";
  std::filesystem::create_directories(userData);

  const auto windowRef
    = CoreWebView2ControllerWindowReference::CreateFromWindowHandle(
      reinterpret_cast<uint64_t>(mBrowserWindow.get()));

  const auto edgeArgs
    = std::format(L"--disable-gpu-vsync --max-gum-fps={}", FramesPerSecond);
  CoreWebView2EnvironmentOptions options;
  options.AdditionalBrowserArguments(edgeArgs);

  mEnvironment = co_await CoreWebView2Environment::CreateWithOptionsAsync(
    {}, userData.wstring(), options);

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
    std::bind_front(&WebView2PageSource::OnWebMessageReceived, this));

  co_await this->ImportJavascriptFile(
    Filesystem::GetImmutableDataDirectory() / "WebView2.js");

  const nlohmann::json initData {
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
        {"IsGitHubActionBuild", Version::IsGithubActionsBuild},
        {"IsTaggedVersion", Version::IsTaggedVersion},
        {"IsStableRelease", Version::IsStableRelease},
      },
    },
  };

  co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
    std::format(L"window.OpenKneeboard = new OpenKneeboardAPI({});", initData));

  if (mSettings.mIntegrateWithSimHub) {
    co_await this->ImportJavascriptFile(
      Filesystem::GetImmutableDataDirectory() / "WebView2-SimHub.js");
    co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
      L"window.OpenKneeboard.SimHubHooks = new OpenKneeboardSimHubHooks();");
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
    "WebView2PageSource::InitializeContentToCapture",
    TraceLoggingValue("Finished", "Result"));
}

winrt::fire_and_forget WebView2PageSource::OnWebMessageReceived(
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
      auto self = std::static_pointer_cast<WebView2PageSource>(weak.lock());
      if (!self) {
        co_return;
      }
      nlohmann::json response {
        {"OpenKneeboard_WebView2_MessageType", "AsyncResponse"},
        {"callID", callID},
      };
      if (result.has_value()) {
        response.emplace("result", result.value());
      } else {
        response.emplace("error", result.error());
        dprintf("WARNING: WebView2 API error: {}", result.error());
      }

      self->mWebView.PostWebMessageAsJson(winrt::to_hstring(response.dump()));
    },
    mWorkerThread,
    weak,
    callID);

  if (message == "OpenKneeboard/SetPreferredPixelSize") {
    respond(co_await this->OnResizeMessage(parsed.at("messageData")));
    co_return;
  }

  if (message == "OpenKneeboard/EnableExperimentalFeatures") {
    respond(co_await this->OnEnableExperimentalFeaturesMessage(
      parsed.at("messageData")));
    co_return;
  }

  if (message == "OpenKneeboard/GetAvailableExperimentalFeatures") {
    respond(co_await this->OnGetAvailableExperimentalFeaturesMessage(
      parsed.at("messageData")));
    co_return;
  }

  if (message == "OpenKneeboard/SetCursorEventsMode") {
    respond(
      co_await this->OnSetCursorEventsModeMessage(parsed.at("messageData")));
    co_return;
  }

  OPENKNEEBOARD_BREAK;
  respond(std::unexpected(std::format("Invalid JS API request: {}", message)));
}

concurrency::task<WebView2PageSource::OKBPromiseResult>
WebView2PageSource::OnResizeMessage(nlohmann::json args) {
  auto weak = weak_from_this();
  const auto uiThread = mUIThread;

  PixelSize size = {
    args.at("width"),
    args.at("height"),
  };
  if (size.mWidth < 1 || size.mHeight < 1) {
    co_return std::unexpected {"WebView2 requested 0px area, ignoring"};
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
      co_return std::unexpected {
        "Requested size scales down to < 1px in at least 1 dimension"};
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
    co_return std::unexpected {"WebView2 no longer exists"};
  }
  WGCPageSource::ForceResize(size);

  evContentChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();

  co_return success("resized");
}

concurrency::task<WebView2PageSource::OKBPromiseResult>
WebView2PageSource::OnSetCursorEventsModeMessage(nlohmann::json args) {
  auto missingFeature = [](auto feature) {
    return std::unexpected {std::format(
      "SetCursorEventMode() failed - the experimental feature `{}` version "
      "`{}` is required.",
      feature.mName,
      feature.mVersion)};
  };
  if (!std::ranges::contains(
        mEnabledExperimentalFeatures, SetCursorEventsModeFeature)) {
    co_return missingFeature(SetCursorEventsModeFeature);
  }
  const std::string mode = args.at("mode");

  auto success = []() { return nlohmann::json {{"result", "success"}}; };

  if (mode == "MouseEmulation") {
    mCursorEventsMode = CursorEventsMode::MouseEmulation;
    co_return success();
  }

  if (mode == "DoodlesOnly") {
    if (!std::ranges::contains(
          mEnabledExperimentalFeatures, DoodlesOnlyToggleableFeature)) {
      co_return missingFeature(DoodlesOnlyToggleableFeature);
    }
    mCursorEventsMode = CursorEventsMode::DoodlesOnly;
    co_return success();
  }

  if (mode == "Raw") {
    if (!std::ranges::contains(
          mEnabledExperimentalFeatures, RawCursorEventsToggleableFeature)) {
      co_return missingFeature(RawCursorEventsFeature);
    }
    mCursorEventsMode = CursorEventsMode::Raw;
    co_return success();
  }

  co_return std::unexpected(std::format("Unrecognized mode '{}'", mode));
}

concurrency::task<WebView2PageSource::OKBPromiseResult>
WebView2PageSource::OnGetAvailableExperimentalFeaturesMessage(
  nlohmann::json args) {
  if constexpr (Version::IsTaggedVersion) {
    co_return std::unexpected(
      "Listing experimental features is not enabled in tagged "
      "releases of OpenKneeboard");
  }

  co_return nlohmann::json {SupportedExperimentalFeatures};
}

concurrency::task<WebView2PageSource::OKBPromiseResult>
WebView2PageSource::OnEnableExperimentalFeaturesMessage(nlohmann::json args) {
  std::vector<ExperimentalFeature> enabledFeatures;

  for (const auto& featureSpec: args.at("features")) {
    const std::string name = featureSpec.at("name");
    const uint64_t version = featureSpec.at("version");

    if (std::ranges::contains(
          mEnabledExperimentalFeatures, name, &ExperimentalFeature::mName)) {
      co_return std::unexpected(
        std::format("Experimental feature `{}` is already enabled", name));
    }

    const ExperimentalFeature feature {name, version};

    if (!std::ranges::contains(SupportedExperimentalFeatures, feature)) {
      if (!std::ranges::contains(
            SupportedExperimentalFeatures, name, &ExperimentalFeature::mName)) {
        co_return std::unexpected(
          std::format("`{}` is not a recognized experimental feature", name));
      }

      co_return std::unexpected(std::format(
        "`{}` is a recognized experimental feature, but `{}` is not a "
        "supported version",
        name,
        version));
    }

    dprintf(
      "WARNING: JS enabled experimental feature `{}` version `{}`",
      name,
      version);

    mEnabledExperimentalFeatures.push_back(feature);
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

    auto warnObsolete = [this](const auto& feature) {
      const auto warning = std::format(
        "WARNING: enabling an obsolete experimental feature: `{}` version "
        "`{}`",
        feature.mName,
        feature.mVersion);
      dprint(warning);
      this->SendJSLog(warning);
    };

    if (feature == RawCursorEventsFeature) {
      warnObsolete(feature);
      if (mCursorEventsMode != CursorEventsMode::MouseEmulation) {
        co_return std::unexpected(std::format(
          "Can not enable `{}`, as the cursor mode has already been changed "
          "by "
          "this page.",
          name));
      }
      mCursorEventsMode = CursorEventsMode::Raw;
      continue;
    }

    if (feature == DoodlesOnlyFeature) {
      warnObsolete(feature);
      if (mCursorEventsMode != CursorEventsMode::MouseEmulation) {
        co_return std::unexpected(std::format(
          "Can not enable `{}`, as the cursor mode has already been changed "
          "by "
          "this page.",
          name));
      }
      mCursorEventsMode = CursorEventsMode::DoodlesOnly;
      continue;
    }

    const auto message = std::format(
      "OpenKneeboard internal error: `{}` v{} is a recognized but unhandled "
      "experimental feature",
      name,
      version);
    dprint(message);
    OPENKNEEBOARD_BREAK;
    co_return std::unexpected(message);
  }

  co_return nlohmann::json {
    {"result", std::format("enabled {} features", enabledFeatures.size())},
    {"details",
     {
       {"features", enabledFeatures},
     }},
  };
}

void WebView2PageSource::InitializeComposition() noexcept {
  OPENKNEEBOARD_TraceLoggingScope("WebView2PageSource::InitializeComposition");
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

static wchar_t WindowClassName[] {L"OpenKneeboard/WebView2Host"};

void WebView2PageSource::RegisterWindowClass() {
  WNDCLASSW windowClass {
    .style = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = &WebView2PageSource::WindowProc,
    .hInstance = GetModuleHandle(nullptr),
    .lpszClassName = WindowClassName,
  };
  ::RegisterClassW(&windowClass);
}

void WebView2PageSource::CreateBrowserWindow() {
  OPENKNEEBOARD_TraceLoggingScope("WebView2PageSource::CreateBrowserWindow()");

  {
    static std::once_flag sFlag;
    std::call_once(sFlag, &WebView2PageSource::RegisterWindowClass);
  }

  mBrowserWindow = unique_hwnd {CreateWindowExW(
    0,
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

WebView2PageSource::~WebView2PageSource() = default;

winrt::fire_and_forget WebView2PageSource::final_release(
  std::unique_ptr<WebView2PageSource> self) {
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

    co_await self->mUIThread;
  }
  co_await self->mDQC.ShutdownQueueAsync();
  co_await self->mUIThread;
  WGCPageSource::final_release(std::move(self));
  co_return;
}

std::optional<float> WebView2PageSource::GetHDRWhiteLevelInNits() const {
  return {};
}

winrt::Windows::Graphics::DirectX::DirectXPixelFormat
WebView2PageSource::GetPixelFormat() const {
  return winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
    B8G8R8A8UIntNormalized;
}

winrt::Windows::Graphics::Capture::GraphicsCaptureItem
WebView2PageSource::CreateWGCaptureItem() {
  return winrt::Windows::Graphics::Capture::GraphicsCaptureItem::
    CreateFromVisual(mRootVisual);
}

PixelRect WebView2PageSource::GetContentRect(const PixelSize& captureSize) {
  return {{0, 0}, mSize};
}

PixelSize WebView2PageSource::GetSwapchainDimensions(
  const PixelSize& captureSize) {
  return captureSize;
}

void WebView2PageSource::PostFrame() {
  this->FlushCursorEvents();
}

void WebView2PageSource::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& event,
  PageID pageID) {
  if (!mController) {
    return;
  }

  switch (mCursorEventsMode) {
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
    case CursorEventsMode::DoodlesOnly:
      if (!mDoodles) {
        mDoodles = std::make_unique<DoodleRenderer>(mDXResources, mKneeboard);
        AddEventListener(
          mDoodles->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
        AddEventListener(
          mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent);
      }
      mDoodles->PostCursorEvent(ctx, event, pageID, mSize);
      return;
  }
}

winrt::fire_and_forget WebView2PageSource::SendJSEvent(
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

winrt::fire_and_forget WebView2PageSource::ActivateJSAPI(std::string_view api) {
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

winrt::fire_and_forget WebView2PageSource::FlushCursorEvents() {
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

bool WebView2PageSource::CanClearUserInput(PageID pageID) const {
  return mDoodles && mDoodles->HaveDoodles(pageID);
}

bool WebView2PageSource::CanClearUserInput() const {
  return mDoodles && mDoodles->HaveDoodles();
}

void WebView2PageSource::ClearUserInput(PageID pageID) {
  if (!mDoodles) {
    return;
  }
  mDoodles->ClearPage(pageID);
}

void WebView2PageSource::ClearUserInput() {
  if (!mDoodles) {
    return;
  }
  mDoodles->Clear();
}

winrt::Windows::Foundation::IAsyncAction
WebView2PageSource::ImportJavascriptFile(std::filesystem::path path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  const auto js = ss.str();

  co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
    winrt::to_hstring(js));
}

void WebView2PageSource::RenderPage(
  RenderTarget* rt,
  PageID page,
  const PixelRect& rect) {
  WGCPageSource::RenderPage(rt, page, rect);

  if (!mDoodles) {
    return;
  }
  mDoodles->Render(rt, page, rect);
}

LRESULT CALLBACK WebView2PageSource::WindowProc(
  HWND const window,
  UINT const message,
  WPARAM const wparam,
  LPARAM const lparam) noexcept {
  return DefWindowProc(window, message, wparam, lparam);
}

}// namespace OpenKneeboard
