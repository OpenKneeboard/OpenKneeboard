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

#include "IPageSourceWithCursorEvents.hpp"

#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/KneeboardViewID.hpp>
#include <OpenKneeboard/WebView2Renderer.hpp>

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include <winrt/Windows.UI.Composition.h>

#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/task.hpp>

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
    public IHasDisposeAsync,
    public EventReceiver,
    public std::enable_shared_from_this<WebView2PageSource> {
 public:
  using Settings = WebView2Renderer::Settings;

  WebView2PageSource() = delete;
  virtual ~WebView2PageSource();

  virtual task<void> DisposeAsync() noexcept override;

  static bool IsAvailable();
  static std::string GetVersion();

  static task<std::shared_ptr<WebView2PageSource>>
  Create(const audited_ptr<DXResources>&, KneeboardState*, const Settings&);

  static task<std::shared_ptr<WebView2PageSource>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const std::filesystem::path&);

  virtual void PostCursorEvent(KneeboardViewID, const CursorEvent&, PageID)
    override;
  [[nodiscard]] task<void>
  RenderPage(const RenderContext&, PageID, const PixelRect& rect) override;

  virtual bool CanClearUserInput(PageID) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  void PostCustomAction(
    KneeboardViewID,
    std::string_view id,
    const nlohmann::json& arg);

  virtual PageIndex GetPageCount() const override;
  virtual std::vector<PageID> GetPageIDs() const override;
  virtual PreferredSize GetPreferredSize(PageID) override;

 private:
  using APIPage = WebView2Renderer::APIPage;
  using InstanceID = WebView2Renderer::InstanceID;
  using ContentMode = WebView2Renderer::ContentMode;
  using RendererKey = KneeboardViewID;
  using WorkerDQ = WebView2Renderer::WorkerDQ;
  using WorkerDQC = WebView2Renderer::WorkerDQC;

  WebView2PageSource(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const Settings&);

  DisposalState mDisposal;

  task<void> Init();

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};
  Settings mSettings;

  enum class RenderersState {
    Constructed,
    Initializing,
    Ready,
    ChangingModes,
  };
  StateMachine<
    RenderersState,
    RenderersState::Constructed,
    std::array {
      Transition {RenderersState::Constructed, RenderersState::Initializing},
      Transition {RenderersState::Initializing, RenderersState::Ready},
      Transition {RenderersState::Ready, RenderersState::ChangingModes},
      Transition {RenderersState::ChangingModes, RenderersState::Ready},
    }>
    mRenderersState;

  WorkerDQC mWorkerDQC {nullptr};
  WorkerDQ mWorkerDQ {nullptr};
  winrt::apartment_context mUIThread;

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

  OpenKneeboard::fire_and_forget OnJSAPI_SetPages(std::vector<APIPage> pages);
  void OnJSAPI_SendMessageToPeers(const InstanceID&, const nlohmann::json&);

  void ConnectRenderer(WebView2Renderer*);
};
}// namespace OpenKneeboard