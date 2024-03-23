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

#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/WebView2PageSource.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/hresult.h>
#include <OpenKneeboard/version.h>

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

WebView2PageSource::WebView2PageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const Settings& settings)
  : WGCPageSource(dxr, kbs, {}),
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
  OPENKNEEBOARD_TraceLoggingScope(
    "WebView2PageSource::InitializeContentToCapture");
  if (!IsAvailable()) {
    co_return;
  }

  co_await winrt::resume_foreground(mDQC.DispatcherQueue());

  mWorkerThread = {};

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

  if (mSettings.mIntegrateWithSimHub) {
    settings.IsWebMessageEnabled(true);
    mWebView.WebMessageReceived(
      std::bind_front(&WebView2PageSource::OnWebMessageReceived, this));

    const auto path
      = Filesystem::GetImmutableDataDirectory() / "WebView2-SimHub.js";

    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    const auto js = ss.str();

    co_await mWebView.AddScriptToExecuteOnDocumentCreatedAsync(
      winrt::to_hstring(js));
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
}

winrt::fire_and_forget WebView2PageSource::OnWebMessageReceived(
  const winrt::Microsoft::Web::WebView2::Core::CoreWebView2&,
  const winrt::Microsoft::Web::WebView2::Core::
    CoreWebView2WebMessageReceivedEventArgs& args) {
  const auto json = to_string(args.WebMessageAsJson());
  const auto parsed = nlohmann::json::parse(json);

  if (!parsed.contains("message")) {
    co_return;
  }

  const std::string message = parsed.at("message");

  if (message != "OpenKneeboard/SimHub/DashboardLoaded") {
    co_return;
  }

  const PixelSize size = {
    parsed.at("data").at("width"),
    parsed.at("data").at("height"),
  };
  if (
    size.mWidth < 1 || size.mHeight < 1
    || size.mWidth > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
    || size.mHeight > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
    dprintf(
      "WebView2 requested size {}x{} is outside of D3D11 limits",
      size.mWidth,
      size.mHeight);
    co_return;
  }
  mSize = size;

  const auto wfSize
    = mSize.StaticCast<float, winrt::Windows::Foundation::Numerics::float2>();
  mRootVisual.Size(wfSize);
  mWebViewVisual.Size(wfSize);
  mController.Bounds({0, 0, mSize.Width<float>(), mSize.Height<float>()});
  mController.RasterizationScale(1.0);
  WGCPageSource::ForceResize(size);

  co_await mUIThread;

  evContentChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();
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
  EventContext,
  const CursorEvent& event,
  PageID) {
  if (!mController) {
    return;
  }
  std::unique_lock lock(mCursorEventsMutex);
  mCursorEvents.push(event);
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

    if (event.mTouchState == CursorTouchState::NOT_NEAR_SURFACE) {
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

bool WebView2PageSource::CanClearUserInput(PageID) const {
  return false;
}

bool WebView2PageSource::CanClearUserInput() const {
  return false;
}

void WebView2PageSource::ClearUserInput(PageID) {
}

void WebView2PageSource::ClearUserInput() {
}

LRESULT CALLBACK WebView2PageSource::WindowProc(
  HWND const window,
  UINT const message,
  WPARAM const wparam,
  LPARAM const lparam) noexcept {
  return DefWindowProc(window, message, wparam, lparam);
}

}// namespace OpenKneeboard
