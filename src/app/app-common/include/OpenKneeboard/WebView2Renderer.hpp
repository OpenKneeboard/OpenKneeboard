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

#include <OpenKneeboard/AppSettings.hpp>
#include <OpenKneeboard/Geometry2D.hpp>
#include <OpenKneeboard/StateMachine.hpp>
#include <OpenKneeboard/WGCRenderer.hpp>

#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include <winrt/Windows.UI.Composition.h>

#include <wil/cppwinrt.h>

#include <expected>
#include <queue>

#include <wil/cppwinrt_helpers.h>

namespace OpenKneeboard {
struct CursorEvent;

class DoodleRenderer;

struct ExperimentalFeature {
  std::string mName;
  uint64_t mVersion;

  bool operator==(const ExperimentalFeature&) const noexcept = default;
};

#define OPENKNEEBOARD_JSAPI_METHODS(IT) \
  IT(EnableExperimentalFeatures) \
  IT(GetPages) \
  IT(SendMessageToPeers) \
  IT(SetCursorEventsMode) \
  IT(SetPages) \
  IT(RequestPageChange) \
  IT(SetPreferredPixelSize) \
  IT(OpenDeveloperToolsWindow)

class WebView2Renderer final : public WGCRenderer {
 public:
  // CoreWebView2 requires these rather than OpenKneeboards' usual
  // Microsoft::UI::Dispatching versions
  using WorkerDQ = winrt::Windows::System::DispatcherQueue;
  using WorkerDQC = winrt::Windows::System::DispatcherQueueController;
  template <class T>
  using worker_task = basic_task<WorkerDQ, T>;
  using JSAPIResult = std::expected<nlohmann::json, std::string>;
  using jsapi_task = worker_task<JSAPIResult>;

  enum class Kind {
    WebDashboard,
    File,// from SingleFileTab or FolderTab
    Plugin,
  };

  struct Settings {
    PixelSize mInitialSize {1024, 768};
    bool mIntegrateWithSimHub {true};
    std::string mURI;
    bool mOpenDeveloperToolsWindow {false};
    bool mTransparentBackground {true};

    ///// NOT SAVED - JUST FOR INTERNAL USE (e.g. PluginTab) /////
    std::unordered_map<std::string, std::filesystem::path> mVirtualHosts;

    constexpr bool operator==(const Settings&) const noexcept = default;
  };

  enum class CursorEventsMode {
    MouseEmulation,
    Raw,
    DoodlesOnly,
  };

  enum class ContentMode {
    Scrollable,
    PageBased,
  };

  struct APIPage {
    ///// Provided by API client /////

    winrt::guid mGuid;
    PixelSize mPixelSize;
    nlohmann::json mExtraData;

    ///// Internals /////

    PageID mPageID;

    bool operator==(const APIPage&) const noexcept = default;
  };

  WebView2Renderer() = delete;
  ~WebView2Renderer();

  // Call before destruction in order to safely release shared resources
  task<void> DisposeAsync() noexcept override;

  static task<std::shared_ptr<WebView2Renderer>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    Kind,
    const Settings&,
    const std::shared_ptr<DoodleRenderer>&,
    const WorkerDQC& workerDQC,
    const winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment&,
    /** nullptr is expected here, unless in page-based mode.
     *
     * In scrollable mode (default), there is a single WebView2Renderer instance
     * In page-based mode (JS API calls), there is a WebView2Renderer instance
     * per view.
     *
     * This is currently (2024-07-20) used for putting helpful messages in the
     * console log.
     */
    KneeboardView*,
    const std::vector<APIPage>&);

  void PostCursorEvent(KneeboardViewID, const CursorEvent&);

  task<void>
  RenderPage(const RenderContext&, PageID page, const PixelRect& rect);

  void PostCustomAction(std::string_view id, const nlohmann::json& arg);

  Event<std::vector<APIPage>> evJSAPI_SetPages;
  void OnJSAPI_Peer_SetPages(const std::vector<APIPage>& pages);

  using InstanceID = winrt::guid;
  Event<InstanceID, nlohmann::json> evJSAPI_SendMessageToPeers;
  Event<KneeboardViewID, PageID> evJSAPI_PageChangeRequested;
  void OnJSAPI_Peer_SendMessageToPeers(
    const InstanceID&,
    const nlohmann::json&);

  Event<std::string> evDocumentTitleChangedEvent;

 protected:
  virtual task<void> InitializeContentToCapture() override;
  virtual std::optional<float> GetHDRWhiteLevelInNits() const override;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const override;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem() override;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) const override;
  virtual PixelSize GetSwapchainDimensions(
    const PixelSize& captureSize) const override;
  virtual void PostFrame() override;

 private:
  using WGCRenderer::Render;
  WebView2Renderer(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    Kind,
    const Settings&,
    const std::shared_ptr<DoodleRenderer>&,
    const WorkerDQC& workerDQC,
    const winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment&,
    KneeboardView*,
    const std::vector<APIPage>&);
  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};
  Kind mKind;
  Settings mSettings;
  PixelSize mSize;
  std::shared_ptr<DoodleRenderer> mDoodles;

  WorkerDQC mDQC {nullptr};
  winrt::apartment_context mUIThread;

  enum class State {
    Constructed,
    InitializedComposition,
    Disposing,
    Disposed,
  };
  AtomicStateMachine<
    State,
    State::Constructed,
    std::array {
      Transition {State::Constructed, State::InitializedComposition},
      Transition {State::InitializedComposition, State::Disposing},
      Transition {State::Disposing, State::Disposed},
    },
    State::Disposed>
    mState;

  inline bool IsLiveForContent() const {
    return mState.Get() == State::InitializedComposition;
  }

  unique_hwnd mBrowserWindow;

  static void RegisterWindowClass();
  /** Not just 'CreateWindow()' because Windows.h's macros interfere */
  void CreateBrowserWindow();
  void InitializeComposition() noexcept;

  winrt::Windows::UI::Composition::Compositor mCompositor {nullptr};
  winrt::Windows::UI::Composition::ContainerVisual mRootVisual {nullptr};
  winrt::Windows::UI::Composition::ContainerVisual mWebViewVisual {nullptr};

  winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment mEnvironment {
    nullptr};
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2CompositionController
    mController {nullptr};
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2 mWebView {nullptr};

  std::mutex mCursorEventsMutex;
  std::chrono::system_clock::time_point mLastCursorEventAt;
  std::queue<CursorEvent> mCursorEvents;
  uint32_t mMouseButtons {};
  OpenKneeboard::fire_and_forget FlushCursorEvents();

  task<void> Resize(PixelSize);

  worker_task<void> ImportJavascriptFile(std::filesystem::path path);

  OpenKneeboard::fire_and_forget OnWebMessageReceived(
    winrt::Microsoft::Web::WebView2::Core::CoreWebView2,
    winrt::Microsoft::Web::WebView2::Core::
      CoreWebView2WebMessageReceivedEventArgs);

#define OKB_DECLARE_JSAPI(FUNC) jsapi_task JSAPI_##FUNC(nlohmann::json args);
  OPENKNEEBOARD_JSAPI_METHODS(OKB_DECLARE_JSAPI)
#undef OKB_DECLARE_JSAPI

  OpenKneeboard::fire_and_forget SendJSLog(auto&&... args) {
    const auto jsArgs
      = nlohmann::json::array({std::forward<decltype(args)>(args)...});
    auto weak = weak_from_this();
    co_await wil::resume_foreground(mDQC.DispatcherQueue());
    auto self = weak.lock();
    if (!self) {
      co_return;
    }

    const nlohmann::json message {
      {"OpenKneeboard_WebView2_MessageType", "console.log"},
      {"logArgs", jsArgs},
    };

    mWebView.PostWebMessageAsJson(winrt::to_hstring(message.dump()));
  }

  OpenKneeboard::fire_and_forget SendJSResponse(uint64_t callID, JSAPIResult);

  OpenKneeboard::fire_and_forget SendJSEvent(
    std::string eventType,
    nlohmann::json eventOptions);

  bool IsJSAPIFeatureEnabled(const ExperimentalFeature&) const;

  JSAPIResult GetJSAPIMissingRequiredFeatureResponse(
    const ExperimentalFeature&,
    const std::source_location& caller = std::source_location::current());

  OpenKneeboard::fire_and_forget ActivateJSAPI(std::string api);

  struct ViewInfo {
    winrt::guid mGuid;
    std::string mName;
    KneeboardViewID mRuntimeID {nullptr};
  };
  std::optional<ViewInfo> mViewInfo {};

  std::vector<APIPage> mInitialPages;

  // Modified by API; should be reset to defaults when navigation starts.
  struct DocumentResources {
    CursorEventsMode mCursorEventsMode {CursorEventsMode::MouseEmulation};
    std::vector<ExperimentalFeature> mEnabledExperimentalFeatures;

    ContentMode mContentMode {ContentMode::Scrollable};

    std::vector<APIPage> mPages;
    PageID mCurrentPage;
  };
  DocumentResources mDocumentResources;

  static LRESULT CALLBACK WindowProc(
    HWND const window,
    UINT const message,
    WPARAM const wparam,
    LPARAM const lparam) noexcept;
};
}// namespace OpenKneeboard

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