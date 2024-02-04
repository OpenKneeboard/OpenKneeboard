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

#include <memory>
#include <vector>

namespace OpenKneeboard {

struct CursorEvent;
class CursorRenderer;
class D2DErrorRenderer;
class ITabView;
class IUILayer;
class KneeboardState;
class TabViewUILayer;
enum class UserAction;
struct CursorEvent;

struct KneeboardViewID final : public UniqueIDBase<KneeboardViewID> {};

enum class CursorPositionState {
  IN_CONTENT_RECT,
  IN_CANVAS_RECT,
  NO_CURSOR_POSITION
};

class KneeboardView final : private EventReceiver,
                            public std::enable_shared_from_this<KneeboardView> {
 public:
  KneeboardView() = delete;
  ~KneeboardView();

  [[nodiscard]] static std::shared_ptr<KneeboardView>
  Create(const audited_ptr<DXResources>&, KneeboardState*, const winrt::guid&);

  virtual winrt::guid GetPersistentGUID() const;
  virtual KneeboardViewID GetRuntimeID() const;

  void SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs);
  void PostUserAction(UserAction);

  virtual std::shared_ptr<ITabView> GetCurrentTabView() const;
  virtual std::shared_ptr<ITab> GetCurrentTab() const;
  virtual TabIndex GetTabIndex() const;
  virtual std::shared_ptr<ITabView> GetTabViewByID(ITab::RuntimeID) const;
  virtual void SetCurrentTabByIndex(TabIndex);
  virtual void SetCurrentTabByRuntimeID(ITab::RuntimeID);

  virtual void PreviousTab();
  virtual void NextTab();

  struct IPCRenderLayout {
    PixelSize mSize;
    PixelRect mContent;
  };

  virtual IPCRenderLayout GetIPCRenderLayout() const;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  virtual PreferredSize GetPreferredSize() const;

  virtual void RenderWithChrome(
    RenderTarget*,
    const D2D1_RECT_F& rect,
    bool isActiveForInput) noexcept;
  virtual std::optional<D2D1_POINT_2F> GetCursorCanvasPoint() const;
  virtual std::optional<D2D1_POINT_2F> GetCursorContentPoint() const;
  virtual D2D1_POINT_2F GetCursorCanvasPoint(
    const D2D1_POINT_2F& contentPoint) const;
  virtual void PostCursorEvent(const CursorEvent& ev);

  virtual std::vector<Bookmark> GetBookmarks() const;
  virtual void RemoveBookmark(const Bookmark&);
  virtual void GoToBookmark(const Bookmark&);

  virtual std::optional<Bookmark> AddBookmarkForCurrentPage();
  virtual void RemoveBookmarkForCurrentPage();
  virtual bool CurrentPageHasBookmark() const;

  virtual void GoToPreviousBookmark();
  virtual void GoToNextBookmark();

  Event<TabIndex> evCurrentTabChangedEvent;
  // TODO - cursor and repaint?
  Event<> evNeedsRepaintEvent;
  Event<CursorEvent> evCursorEvent;
  Event<> evLayoutChangedEvent;
  Event<> evBookmarksChangedEvent;

 private:
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
    const winrt::guid&);
  winrt::apartment_context mUIThread;
  KneeboardViewID mID;
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;
  EventContext mEventContext;
  std::vector<std::shared_ptr<ITabView>> mTabViews;
  std::shared_ptr<ITabView> mCurrentTabView;

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

  winrt::guid mGUID;
};

}// namespace OpenKneeboard
