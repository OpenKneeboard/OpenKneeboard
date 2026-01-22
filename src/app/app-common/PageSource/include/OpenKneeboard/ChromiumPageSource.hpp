// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "IPageSourceWithCursorEvents.hpp"

#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/D3D11/SpriteBatch.hpp>
#include <OpenKneeboard/IPageSourceWithDeveloperTools.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/KneeboardViewID.hpp>
#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/WebPageSourceKind.hpp>
#include <OpenKneeboard/WebPageSourceSettings.hpp>

#include <Windows.h>

#include <wil/com.h>

// TODO: Avoid the need to include this, to avoid poluting with the generic
// macros like `IMPLEMENT_REFCOUNTING`
#include <include/cef_base.h>
#include <include/cef_client.h>

#include <shared_mutex>

namespace OpenKneeboard {

class DoodleRenderer;

/// A browser using Chromium Embedded Framework
class ChromiumPageSource final
  : public virtual IPageSourceWithInternalCaching,
    public virtual IPageSourceWithDeveloperTools,
    public virtual IPageSourceWithCursorEvents,
    public EventReceiver,
    public std::enable_shared_from_this<ChromiumPageSource> {
 public:
  using Kind = WebPageSourceKind;
  using Settings = WebPageSourceSettings;

  ChromiumPageSource() = delete;
  virtual ~ChromiumPageSource();

  static task<std::shared_ptr<ChromiumPageSource>>
  Create(audited_ptr<DXResources>, KneeboardState*, Kind, Settings);
  static task<std::shared_ptr<ChromiumPageSource>>
  Create(audited_ptr<DXResources>, KneeboardState*, std::filesystem::path);

  void PostCursorEvent(KneeboardViewID, const CursorEvent&, PageID) override;
  task<void> RenderPage(RenderContext, PageID, PixelRect rect) override;

  bool CanClearUserInput(PageID) const override;
  bool CanClearUserInput() const override;
  void ClearUserInput(PageID) override;
  void ClearUserInput() override;

  PageIndex GetPageCount() const override;
  std::vector<PageID> GetPageIDs() const override;
  std::optional<PreferredSize> GetPreferredSize(PageID) override;

  Event<std::string> evDocumentTitleChangedEvent;

  void PostCustomAction(
    KneeboardViewID,
    std::string_view,
    const nlohmann::json& arg);

  bool HasDeveloperTools(PageID) const override { return true; }
  fire_and_forget OpenDeveloperToolsWindow(KneeboardViewID, PageID) override;

 private:
  class Client;
  class RenderHandler;
  struct APIPage {
    ///// Provided by API client /////

    winrt::guid mGuid;
    PixelSize mPixelSize;
    nlohmann::json mExtraData;

    ///// Internals /////

    PageID mPageID;

    bool operator==(const APIPage&) const noexcept = default;
  };
  friend void to_json(nlohmann::json&, const APIPage&);
  friend void from_json(const nlohmann::json&, APIPage&);

  struct ScrollableState {
    CefRefPtr<Client> mClient;
  };
  struct PageBasedState {
    CefRefPtr<Client> mPrimaryClient;
    std::vector<APIPage> mPages;
    std::unordered_map<KneeboardViewID, CefRefPtr<Client>> mClients;
  };

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};
  Kind mKind {};
  Settings mSettings {};
  D3D11::SpriteBatch mSpriteBatch;
  std::unique_ptr<DoodleRenderer> mDoodles;

  mutable std::shared_mutex mStateMutex;
  std::variant<ScrollableState, PageBasedState> mState;

  ChromiumPageSource(audited_ptr<DXResources>, KneeboardState*, Kind, Settings);
  void Init();

  CefRefPtr<Client> GetOrCreateClient(KneeboardViewID);
  CefRefPtr<Client> CreateClient(
    std::optional<KneeboardViewID> viewID = std::nullopt);
};
}// namespace OpenKneeboard
