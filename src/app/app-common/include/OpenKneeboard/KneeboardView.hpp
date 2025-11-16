// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Bookmark.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardViewID.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/inttypes.hpp>

#include <memory>
#include <source_location>
#include <vector>

namespace OpenKneeboard {

struct CursorEvent;
class CursorRenderer;
class D2DErrorRenderer;
class TabView;
class IUILayer;
class KneeboardState;
class TabViewUILayer;
enum class UserAction;
struct CursorEvent;

class KneeboardView final : private EventReceiver,
                            public std::enable_shared_from_this<KneeboardView> {
 public:
  KneeboardView() = delete;
  ~KneeboardView();

  [[nodiscard]] static std::shared_ptr<KneeboardView> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid&,
    std::string_view name);

  winrt::guid GetPersistentGUID() const noexcept;
  KneeboardViewID GetRuntimeID() const noexcept;
  std::string_view GetName() const noexcept;

  void SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs);
  [[nodiscard]]
  task<void> PostUserAction(UserAction);

  std::shared_ptr<TabView> GetCurrentTabView() const;
  std::weak_ptr<ITab> GetCurrentTab() const;
  TabIndex GetTabIndex() const;
  std::shared_ptr<TabView> GetTabViewByID(ITab::RuntimeID) const;
  void SetCurrentTabByIndex(
    TabIndex,
    const std::source_location& loc = std::source_location::current());
  void SetCurrentTabByRuntimeID(
    ITab::RuntimeID,
    const std::source_location& loc = std::source_location::current());

  void PreviousTab();
  void NextTab();

  struct IPCRenderLayout {
    PixelSize mSize;
    PixelRect mContent;
  };

  IPCRenderLayout GetIPCRenderLayout() const;
  /** ContentRenderRect may be scaled; this is the 'real' size.
   *
   * This is *not* an `std::optional<>`, as even without a tab/page, a real size
   * is needed to map tablet input to toolbars/menus.
   */
  PreferredSize GetPreferredSize() const;

  [[nodiscard]] task<void> RenderWithChrome(
    RenderTarget*,
    const PixelRect& rect,
    bool isActiveForInput) noexcept;
  std::optional<D2D1_POINT_2F> GetCursorCanvasPoint() const;
  std::optional<D2D1_POINT_2F> GetCursorContentPoint() const;
  D2D1_POINT_2F GetCursorCanvasPoint(const D2D1_POINT_2F& contentPoint) const;
  void PostCursorEvent(const CursorEvent& ev);

  std::vector<winrt::guid> GetTabIDs() const noexcept;

  std::vector<Bookmark> GetBookmarks() const;
  void RemoveBookmark(const Bookmark&);
  void GoToBookmark(const Bookmark&);

  std::optional<Bookmark> AddBookmarkForCurrentPage();
  void RemoveBookmarkForCurrentPage();
  bool CurrentPageHasBookmark() const;

  void GoToPreviousBookmark();
  void GoToNextBookmark();

  void PostCustomAction(std::string_view id, const nlohmann::json& arg);

  // Not just overloading std::swap because this intentionally does not swap IDs
  void SwapState(KneeboardView& other);

  Event<TabIndex> evCurrentTabChangedEvent;
  // TODO - cursor and repaint?
  Event<> evNeedsRepaintEvent;
  Event<CursorEvent> evCursorEvent;
  Event<> evLayoutChangedEvent;
  Event<> evBookmarksChangedEvent;

 private:
  void SetTabViews(
    std::vector<std::shared_ptr<TabView>>&& views,
    const std::shared_ptr<TabView>& currentView);

  void UpdateUILayers();
  enum class RelativePosition {
    Previous,
    Next,
  };
  std::optional<Bookmark> GetBookmark(RelativePosition) const;
  void SetBookmark(RelativePosition);
  std::vector<std::shared_ptr<ITab>> GetRootTabs() const;

  KneeboardView(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid&,
    std::string_view name);

  winrt::apartment_context mUIThread;
  const KneeboardViewID mRuntimeID;
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;
  std::vector<std::shared_ptr<TabView>> mTabViews;
  std::shared_ptr<TabView> mCurrentTabView;

  std::optional<D2D1_POINT_2F> mCursorCanvasPoint;

  std::unique_ptr<CursorRenderer> mCursorRenderer;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;

  winrt::com_ptr<ID2D1SolidColorBrush> mErrorBackgroundBrush;

  std::vector<IUILayer*> mUILayers {};
  std::shared_ptr<IUILayer> mHeaderUILayer;
  std::unique_ptr<IUILayer> mFooterUILayer;
  std::shared_ptr<IUILayer> mBookmarksUILayer;
  std::unique_ptr<TabViewUILayer> mTabViewUILayer;

  std::vector<EventHandlerToken> mTabEvents;

  std::tuple<IUILayer*, std::span<IUILayer*>> GetUILayers() const;

  ThreadGuard mThreadGuard;

  winrt::guid mGuid;
  std::string mName;
};

}// namespace OpenKneeboard
