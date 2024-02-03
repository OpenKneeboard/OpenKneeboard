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

#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/hresult.h>
#include <OpenKneeboard/version.h>

#include <Windows.h>

#include <future>
#include <system_error>

#include <WebView2.h>
#include <wrl.h>

namespace OpenKneeboard {
std::shared_ptr<WebView2PageSource> WebView2PageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs) {
  auto ret = shared_with_final_release(new WebView2PageSource(dxr, kbs));
  ret->Init();
  return ret;
}

WebView2PageSource::WebView2PageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : WGCPageSource(dxr, kbs, {}) {
  OPENKNEEBOARD_TraceLoggingScope("WebView2PageSource::WebView2PageSource()");
  if (!IsAvailable()) {
    return;
  }
}

winrt::Windows::Foundation::IAsyncAction
WebView2PageSource::InitializeInCaptureThread() {
  OPENKNEEBOARD_TraceLoggingScope(
    "WebView2PageSource::InitializeInCaptureThread");
  if (!IsAvailable()) {
    co_return;
  }

  this->CreateBrowserWindow();
  this->InitializeComposition();

  using namespace winrt::Microsoft::Web::WebView2::Core;
  CoreWebView2EnvironmentOptions options;

  const auto userData = Filesystem::GetLocalAppDataDirectory() / "WebView2";
  std::filesystem::create_directories(userData);

  const auto windowRef
    = CoreWebView2ControllerWindowReference::CreateFromWindowHandle(
      reinterpret_cast<uint64_t>(mBrowserWindow.get()));

  mEnvironment = co_await CoreWebView2Environment::CreateWithOptionsAsync(
    {}, userData.wstring(), {});
  mController
    = co_await mEnvironment.CreateCoreWebView2CompositionControllerAsync(
      windowRef);
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

  mWebView.Navigate(L"https://www.testufo.com");

  co_return;
}

void WebView2PageSource::InitializeComposition() {
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
    auto e = std::system_category().default_error_condition(GetLastError());
    dprintf("Failed to create browser window: {} - {}", e.value(), e.message());
    OPENKNEEBOARD_BREAK;
  }
}

WebView2PageSource::~WebView2PageSource() = default;

winrt::fire_and_forget WebView2PageSource::final_release(
  std::unique_ptr<WebView2PageSource> self) {
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
  return {{0, 0}, captureSize};
}

PixelSize WebView2PageSource::GetSwapchainDimensions(
  const PixelSize& captureSize) {
  return captureSize;
}

LRESULT CALLBACK WebView2PageSource::WindowProc(
  HWND const window,
  UINT const message,
  WPARAM const wparam,
  LPARAM const lparam) noexcept {
  return DefWindowProc(window, message, wparam, lparam);
}

}// namespace OpenKneeboard
