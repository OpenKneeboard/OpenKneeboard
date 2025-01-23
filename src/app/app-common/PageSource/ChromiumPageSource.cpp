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

#include <include/cef_client.h>

namespace OpenKneeboard {

class ChromiumPageSource::Client final : public CefClient,
                                         public CefLifeSpanHandler {
 public:
  Client() {
  }
  ~Client() {
  }

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
    return this;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    mBrowser = browser;
    mBrowserId = mBrowser->GetIdentifier();
  }

  CefRefPtr<CefBrowser> GetBrowser() const {
    return mBrowser;
  }

  std::optional<int> GetBrowserID() const {
    return mBrowserId;
  }

 private:
  IMPLEMENT_REFCOUNTING(Client);

  CefRefPtr<CefBrowser> mBrowser;
  std::optional<int> mBrowserId;
};

ChromiumPageSource::~ChromiumPageSource() = default;

task<std::shared_ptr<ChromiumPageSource>> ChromiumPageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs) {
  std::shared_ptr<ChromiumPageSource> ret(new ChromiumPageSource(dxr, kbs));
  co_await ret->Init();
  co_return ret;
}

task<void> ChromiumPageSource::Init() {
  CefWindowInfo info {};

  CefBrowserSettings settings {};

  mClient = {new Client()};

  CefBrowserHost::CreateBrowser(
    info, mClient, "https://example.com", settings, nullptr, nullptr);
  co_return;
}

task<void> ChromiumPageSource::DisposeAsync() noexcept {
  auto disposing = co_await mDisposal.StartOnce();
  co_return;
}

void ChromiumPageSource::PostCursorEvent(
  KneeboardViewID,
  const CursorEvent&,
  PageID) {
  // TODO
}

task<void>
ChromiumPageSource::RenderPage(RenderContext, PageID, PixelRect rect) {
  // TODO
  co_return;
}

bool ChromiumPageSource::CanClearUserInput(PageID) const {
  return CanClearUserInput();
}

bool ChromiumPageSource::CanClearUserInput() const {
  // TODO
  return false;
}

void ChromiumPageSource::ClearUserInput(PageID) {
  ClearUserInput();
}

void ChromiumPageSource::ClearUserInput() {
  // TODO
}

PageIndex ChromiumPageSource::GetPageCount() const {
  // TODO
  return 0;
}

std::vector<PageID> ChromiumPageSource::GetPageIDs() const {
  if (this->GetPageCount() == 0) {
    return {};
  }
  return {mPageID};
}

std::optional<PreferredSize> ChromiumPageSource::GetPreferredSize(PageID) {
  // TODO
  return std::nullopt;
}

ChromiumPageSource::ChromiumPageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : mDXResources(dxr), mKneeboard(kbs) {
}

}// namespace OpenKneeboard