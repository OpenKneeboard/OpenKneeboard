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

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/IKneeboardView.h>

#include <vector>

namespace OpenKneeboard {

class CursorRenderer;
class ITabView;
class IUILayer;
class KneeboardState;
class TabViewUILayer;
enum class UserAction;
struct CursorEvent;

class KneeboardView final : public IKneeboardView,
                            private EventReceiver,
                            public std::enable_shared_from_this<KneeboardView> {
 public:
  KneeboardView() = delete;
  ~KneeboardView();

  [[nodiscard]] static std::shared_ptr<KneeboardView> Create(
    const DXResources&,
    KneeboardState*);

  virtual KneeboardViewID GetRuntimeID() const override;

  void SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs);
  void PostUserAction(UserAction);

  virtual std::shared_ptr<ITabView> GetCurrentTabView() const override;
  virtual std::shared_ptr<ITab> GetCurrentTab() const override;
  virtual TabIndex GetTabIndex() const override;
  virtual std::shared_ptr<ITabView> GetTabViewByID(
    ITab::RuntimeID) const override;
  virtual void SetCurrentTabByIndex(TabIndex) override;
  virtual void SetCurrentTabByRuntimeID(ITab::RuntimeID) override;

  virtual void PreviousTab() override;
  virtual void NextTab() override;

  virtual void NextPage() override;
  virtual void PreviousPage() override;

  virtual D2D1_SIZE_U GetCanvasSize() const override;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  virtual D2D1_SIZE_U GetContentNativeSize() const override;

  virtual void RenderWithChrome(
    RenderTargetID,
    ID2D1DeviceContext* d2d,
    const D2D1_RECT_F& rect,
    bool isActiveForInput) noexcept override;
  virtual std::optional<D2D1_POINT_2F> GetCursorCanvasPoint() const override;
  virtual std::optional<D2D1_POINT_2F> GetCursorContentPoint() const override;
  virtual D2D1_POINT_2F GetCursorCanvasPoint(
    const D2D1_POINT_2F& contentPoint) const override;
  virtual void PostCursorEvent(const CursorEvent& ev) override;

  virtual std::vector<Bookmark> GetBookmarks() const override;
  virtual void RemoveBookmark(const Bookmark&) override;
  virtual void GoToBookmark(const Bookmark&) override;

  virtual std::optional<Bookmark> AddBookmarkForCurrentPage() override;
  virtual void RemoveBookmarkForCurrentPage() override;
  virtual void ToggleBookmarkForCurrentPage() override;
  virtual bool CurrentPageHasBookmark() const override;

  virtual void GoToPreviousBookmark() override;
  virtual void GoToNextBookmark() override;

 private:
  void UpdateUILayers();
  enum class RelativePosition {
    Previous,
    Next,
  };
  std::optional<Bookmark> GetBookmark(RelativePosition) const;
  void SetBookmark(RelativePosition);

  KneeboardView(const DXResources&, KneeboardState*);
  winrt::apartment_context mUIThread;
  KneeboardViewID mID;
  DXResources mDXR;
  KneeboardState* mKneeboard;
  EventContext mEventContext;
  std::vector<std::shared_ptr<ITabView>> mTabViews;
  std::shared_ptr<ITabView> mCurrentTabView;

  std::optional<D2D1_POINT_2F> mCursorCanvasPoint;

  std::unique_ptr<CursorRenderer> mCursorRenderer;

  std::vector<IUILayer*> mUILayers {};
  std::shared_ptr<IUILayer> mHeaderUILayer;
  std::unique_ptr<IUILayer> mFooterUILayer;
  std::shared_ptr<IUILayer> mBookmarksUILayer;
  std::unique_ptr<TabViewUILayer> mTabViewUILayer;

  std::vector<EventHandlerToken> mTabEvents;

  std::tuple<IUILayer*, std::span<IUILayer*>> GetUILayers() const;
};

}// namespace OpenKneeboard
