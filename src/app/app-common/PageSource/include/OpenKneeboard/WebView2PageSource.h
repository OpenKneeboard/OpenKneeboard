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

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/WebView2Renderer.h>

#include <OpenKneeboard/handles.h>

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include <winrt/Windows.UI.Composition.h>

#include <expected>
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
class WebView2PageSource final
  : public virtual IPageSourceWithInternalCaching,
    public virtual IPageSourceWithCursorEvents,
    public EventReceiver,
    public std::enable_shared_from_this<WebView2PageSource> {
 public:
  using Settings = WebView2Renderer::Settings;

  WebView2PageSource() = delete;
  virtual ~WebView2PageSource();

  static bool IsAvailable();
  static std::string GetVersion();

  static std::shared_ptr<WebView2PageSource>
  Create(const audited_ptr<DXResources>&, KneeboardState*, const Settings&);
  static winrt::fire_and_forget final_release(
    std::unique_ptr<WebView2PageSource>);

  virtual void PostCursorEvent(EventContext, const CursorEvent&, PageID)
    override;
  virtual void RenderPage(const RenderContext&, PageID, const PixelRect& rect)
    override;

  virtual bool CanClearUserInput(PageID) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  virtual PageIndex GetPageCount() const override;
  virtual std::vector<PageID> GetPageIDs() const override;
  virtual PreferredSize GetPreferredSize(PageID) override;

 private:
  using APIPage = WebView2Renderer::APIPage;
  using InstanceID = WebView2Renderer::InstanceID;
  using ContentMode = WebView2Renderer::ContentMode;
  using RendererKey = KneeboardViewID;

  WebView2PageSource(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const Settings&);

  winrt::fire_and_forget Init();

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};
  Settings mSettings;

  winrt::Windows::System::DispatcherQueueController mDQC {nullptr};
  winrt::apartment_context mUIThread;
  winrt::apartment_context mWorkerThread {nullptr};

  winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment mEnvironment {
    nullptr};

  struct DocumentResources {
    std::vector<APIPage> mPages;
    ContentMode mContentMode {ContentMode::Scrollable};
    std::shared_ptr<DoodleRenderer> mDoodles;

    std::unordered_map<RendererKey, std::shared_ptr<WebView2Renderer>>
      mRenderers;
  };

  DocumentResources mDocumentResources {};
  RendererKey mScrollableContentRendererKey;
  PageID mScrollableContentPageID;

  void OnJSAPI_SetPages(const std::vector<APIPage>& pages);
  void OnJSAPI_SendMessageToPeers(const InstanceID&, const nlohmann::json&);
};
}// namespace OpenKneeboard