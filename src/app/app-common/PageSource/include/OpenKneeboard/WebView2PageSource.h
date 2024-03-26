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

#include "IPageSourceWithCursorEvents.h"
#include "WGCPageSource.h"

#include <OpenKneeboard/CursorEvent.h>

#include <OpenKneeboard/handles.h>

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include <winrt/Windows.UI.Composition.h>

#include <mutex>
#include <queue>

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
class WebView2PageSource final : public WGCPageSource,
                                 public IPageSourceWithCursorEvents {
 public:
  WebView2PageSource() = delete;
  virtual ~WebView2PageSource();

  static bool IsAvailable();
  static std::string GetVersion();

  struct Settings {
    PixelSize mInitialSize {1024, 768};
    bool mIntegrateWithSimHub {true};
    std::string mURI;
    bool mOpenDeveloperToolsWindow {false};
    bool mTransparentBackground {true};
    constexpr bool operator==(const Settings&) const noexcept = default;
  };

  static std::shared_ptr<WebView2PageSource>
  Create(const audited_ptr<DXResources>&, KneeboardState*, const Settings&);
  static winrt::fire_and_forget final_release(
    std::unique_ptr<WebView2PageSource>);

  virtual void PostCursorEvent(EventContext, const CursorEvent&, PageID)
    override;

  virtual bool CanClearUserInput(PageID) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

 protected:
  virtual std::optional<float> GetHDRWhiteLevelInNits() const override;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const override;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem() override;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) override;
  virtual PixelSize GetSwapchainDimensions(
    const PixelSize& captureSize) override;
  virtual winrt::Windows::Foundation::IAsyncAction InitializeContentToCapture()
    override;

  virtual void PostFrame() override;

 private:
  WebView2PageSource(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const Settings&);

  winrt::apartment_context mUIThread;
  winrt::apartment_context mWorkerThread {nullptr};

  Settings mSettings;
  PixelSize mSize;

  static void RegisterWindowClass();
  /** Not just 'CreateWindow()' because Windows.h's macros interfere */
  void CreateBrowserWindow();
  void InitializeComposition() noexcept;

  unique_hwnd mBrowserWindow;

  winrt::Windows::UI::Composition::Compositor mCompositor {nullptr};
  winrt::Windows::UI::Composition::ContainerVisual mRootVisual {nullptr};
  winrt::Windows::UI::Composition::ContainerVisual mWebViewVisual {nullptr};

  winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment mEnvironment {
    nullptr};
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2CompositionController
    mController {nullptr};
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2 mWebView {nullptr};

  winrt::Windows::System::DispatcherQueueController mDQC {nullptr};

  std::mutex mCursorEventsMutex;
  std::queue<CursorEvent> mCursorEvents;
  uint32_t mMouseButtons {};

  winrt::fire_and_forget FlushCursorEvents();

  winrt::fire_and_forget OnWebMessageReceived(
    const winrt::Microsoft::Web::WebView2::Core::CoreWebView2&,
    const winrt::Microsoft::Web::WebView2::Core::
      CoreWebView2WebMessageReceivedEventArgs&);

  winrt::Windows::Foundation::IAsyncAction ImportJavascriptFile(const std::filesystem::path& path);

  static LRESULT CALLBACK WindowProc(
    HWND const window,
    UINT const message,
    WPARAM const wparam,
    LPARAM const lparam) noexcept;
};
}// namespace OpenKneeboard