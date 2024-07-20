/* OpenKneeboard
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

#include <OpenKneeboard/Bookmark.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ThreadGuard.h>

#include <OpenKneeboard/audited_ptr.h>
#include <OpenKneeboard/inttypes.h>

#include <shims/source_location>

#include <memory>
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

struct KneeboardViewID final : public UniqueIDBase<KneeboardViewID> {};

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
  void PostUserAction(UserAction);

  std::shared_ptr<TabView> GetCurrentTabView() const;
  std::shared_ptr<ITab> GetCurrentTab() const;
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
  /// ContentRenderRect may be scaled; this is the 'real' size.
  PreferredSize GetPreferredSize() const;

  void RenderWithChrome(
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

  KneeboardView(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid&,
    std::string_view name);
  winrt::apartment_context mUIThread;
  KneeboardViewID mID;
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;
  EventContext mEventContext;
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
