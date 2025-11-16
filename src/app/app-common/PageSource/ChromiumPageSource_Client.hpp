// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ChromiumPageSource.hpp>

#include <optional>

namespace OpenKneeboard {

struct ExperimentalFeature {
  std::string mName;
  uint64_t mVersion;

  bool operator==(const ExperimentalFeature&) const noexcept = default;
};

class ChromiumPageSource::Client final : public CefClient,
                                         public CefLifeSpanHandler,
                                         public CefDisplayHandler,
                                         public CefRequestHandler,
                                         public CefResourceRequestHandler,
                                         public CefDevToolsMessageObserver {
 public:
  using JSAPIResult = std::expected<nlohmann::json, std::string>;
  enum class CursorEventsMode {
    MouseEmulation,
    DoodlesOnly,
  };

  Client() = delete;
  Client(
    std::shared_ptr<ChromiumPageSource> pageSource,
    std::optional<KneeboardViewID>);
  ~Client();

  static nlohmann::json GetSupportedExperimentalFeatures();

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
  CefRefPtr<CefRenderHandler> GetRenderHandler() override;
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
  CefRefPtr<CefRequestHandler> GetRequestHandler() override;

  CefRefPtr<RenderHandler> GetRenderHandlerSubclass();

  void OnDevToolsAgentDetached(CefRefPtr<CefBrowser>) override;

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    const CefString& request_initiator,
    bool& disable_default_handling) override;

  CefRefPtr<CefResourceHandler> GetResourceHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request) override;

  bool OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId process,
    CefRefPtr<CefProcessMessage> message) override;

  bool OnBeforePopup(
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
    bool* no_javascript_access) override;

  void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  CefRefPtr<CefBrowser> GetBrowser() const;
  std::optional<int> GetBrowserID() const;

  void PostCustomAction(std::string_view actionID, const nlohmann::json& arg);
  void PostCursorEvent(const CursorEvent& ev);

  PageID GetCurrentPage() const;
  void SetCurrentPage(PageID, const PixelSize&);

  void SetViewID(KneeboardViewID);

  CursorEventsMode GetCursorEventsMode() const;

  task<JSAPIResult> OpenDeveloperToolsWindow();

 private:
  IMPLEMENT_REFCOUNTING(Client);
  winrt::apartment_context mUIThread;

  wil::unique_handle mShutdownEvent;

  std::weak_ptr<ChromiumPageSource> mPageSource;
  std::optional<KneeboardViewID> mViewID;
  CefRefPtr<CefBrowser> mBrowser;
  CefRefPtr<RenderHandler> mRenderHandler;
  std::optional<int> mBrowserId;

  PageID mCurrentPage;

  CursorEventsMode mCursorEventsMode {CursorEventsMode::MouseEmulation};
  bool mIsHovered = false;
  uint32_t mCursorButtons = 0;
  std::chrono::steady_clock::time_point mLastCursorEventAt;

  std::vector<ExperimentalFeature> mEnabledExperimentalFeatures;

  template <auto TMethod>
  fire_and_forget OnJSAsyncRequest(
    CefRefPtr<CefFrame> frame,
    CefProcessId process,
    CefRefPtr<CefProcessMessage> message);
  void SendJSAsyncResult(
    CefRefPtr<CefFrame> frame,
    CefProcessId process,
    int callID,
    JSAPIResult result);
  void EnableJSAPI(CefString name);

  task<JSAPIResult> SetPreferredPixelSize(uint32_t width, uint32_t height);
  task<JSAPIResult> EnableExperimentalFeatures(
    std::vector<ExperimentalFeature>);
  task<JSAPIResult> SetCursorEventsMode(CursorEventsMode);
  task<JSAPIResult> GetPages();
  task<JSAPIResult> SetPages(std::vector<APIPage>);
  task<JSAPIResult> SendMessageToPeers(nlohmann::json message);
  task<JSAPIResult> RequestPageChange(nlohmann::json);
  task<JSAPIResult> GetGraphicsTabletInfo();
};
}// namespace OpenKneeboard