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
#pragma once

#include <OpenKneeboard/WGCPageSource.h>

#include <OpenKneeboard/handles.h>

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include <winrt/Windows.UI.Composition.h>

#include <WebView2.h>

namespace OpenKneeboard {

/** A browser using WebView2
 *
 * Okay, this is nasty. WebView2 does not support indirect rendering,
 * so we'll create an invisible window, then run
 * Windows::Graphics::Capture on it.
 *
 * It'll be fun, they said.
 */
class WebView2PageSource final : public WGCPageSource {
 public:
  WebView2PageSource() = delete;
  virtual ~WebView2PageSource();

  static bool IsAvailable();
  static std::string GetVersion();

  struct Settings {
    PixelSize mInitialSize {1024, 768};
    bool mAutoResizeForSimHub {true};
    std::string mURI;
    bool mOpenDeveloperToolsWindow {false};
    constexpr bool operator==(const Settings&) const noexcept = default;
  };

  static std::shared_ptr<WebView2PageSource>
  Create(const audited_ptr<DXResources>&, KneeboardState*, const Settings&);
  static winrt::fire_and_forget final_release(
    std::unique_ptr<WebView2PageSource>);

 protected:
  virtual std::optional<float> GetHDRWhiteLevelInNits() const override;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const override;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem() override;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) override;
  virtual PixelSize GetSwapchainDimensions(
    const PixelSize& captureSize) override;
  virtual winrt::Windows::Foundation::IAsyncAction
  WebView2PageSource::InitializeInCaptureThread() override;

 private:
  WebView2PageSource(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const Settings&);

  Settings mSettings;
  PixelSize mSize;

  static void RegisterWindowClass();
  /** Not just 'CreateWindow()' because Windows.h's macros interfere */
  void CreateBrowserWindow();
  void InitializeComposition();

  unique_hwnd mBrowserWindow;

  winrt::Windows::UI::Composition::Compositor mCompositor {nullptr};
  winrt::Windows::UI::Composition::ContainerVisual mRootVisual {nullptr};
  winrt::Windows::UI::Composition::ContainerVisual mWebViewVisual {nullptr};

  winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment mEnvironment {
    nullptr};
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2CompositionController
    mController {nullptr};
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2 mWebView {nullptr};

  void OnWebMessageReceived(
    const winrt::Microsoft::Web::WebView2::Core::CoreWebView2&,
    const winrt::Microsoft::Web::WebView2::Core::
      CoreWebView2WebMessageReceivedEventArgs&);

  static LRESULT CALLBACK WindowProc(
    HWND const window,
    UINT const message,
    WPARAM const wparam,
    LPARAM const lparam) noexcept;
};
}// namespace OpenKneeboard