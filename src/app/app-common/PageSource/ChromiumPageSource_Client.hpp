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
#pragma once

#include <OpenKneeboard/ChromiumPageSource.hpp>

namespace OpenKneeboard {

struct ExperimentalFeature {
  std::string mName;
  uint64_t mVersion;

  bool operator==(const ExperimentalFeature&) const noexcept = default;
};

class ChromiumPageSource::Client final : public CefClient,
                                         public CefLifeSpanHandler,
                                         public CefDisplayHandler {
 public:
  Client() = delete;
  Client(ChromiumPageSource* pageSource);
  ~Client();

  static nlohmann::json GetSupportedExperimentalFeatures();

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
  CefRefPtr<CefRenderHandler> GetRenderHandler() override;
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;

  CefRefPtr<RenderHandler> GetRenderHandlerSubclass();

  bool OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId process,
    CefRefPtr<CefProcessMessage> message) override;

  void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

  CefRefPtr<CefBrowser> GetBrowser() const;
  std::optional<int> GetBrowserID() const;

  void PostCursorEvent(const CursorEvent& ev);

  PageID GetCurrentPage() const;

 private:
  IMPLEMENT_REFCOUNTING(Client);
  using JSAPIResult = std::expected<nlohmann::json, std::string>;

  ChromiumPageSource* mPageSource {nullptr};
  CefRefPtr<CefBrowser> mBrowser;
  CefRefPtr<RenderHandler> mRenderHandler;
  std::optional<int> mBrowserId;

  PageID mCurrentPage;

  bool mIsHovered = false;
  uint32_t mCursorButtons = 0;

  std::vector<ExperimentalFeature> mEnabledExperimentalFeatures;

  template <auto TMethod>
  fire_and_forget OnJSAsyncRequest(CefRefPtr<CefProcessMessage> message);
  void SendJSAsyncResult(int callID, JSAPIResult result);

  task<JSAPIResult> SetPreferredPixelSize(uint32_t width, uint32_t height);
  task<JSAPIResult> EnableExperimentalFeatures(
    std::vector<ExperimentalFeature>);
};
}// namespace OpenKneeboard