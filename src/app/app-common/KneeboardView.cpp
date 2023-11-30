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
#include <OpenKneeboard/BookmarksUILayer.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/CursorRenderer.h>
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/FooterUILayer.h>
#include <OpenKneeboard/HeaderUILayer.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabViewUILayer.h>
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/UserAction.h>
#include <OpenKneeboard/UserActionHandler.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/weak_wrap.h>

#include <algorithm>
#include <ranges>

namespace OpenKneeboard {

KneeboardView::KneeboardView(const DXResources& dxr, KneeboardState* kneeboard)
  : mDXR(dxr), mKneeboard(kneeboard) {
  mCursorRenderer = std::make_unique<CursorRenderer>(dxr);
  mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr);

  dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::White), mErrorBackgroundBrush.put());

  mHeaderUILayer = HeaderUILayer::Create(dxr, kneeboard, this);
  mFooterUILayer = std::make_unique<FooterUILayer>(dxr, kneeboard);
  mBookmarksUILayer = BookmarksUILayer::Create(dxr, kneeboard, this);
  mTabViewUILayer = std::make_unique<TabViewUILayer>(dxr);

  this->UpdateUILayers();

  for (auto layer: mUILayers) {
    AddEventListener(layer->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  }
  AddEventListener(this->evCurrentTabChangedEvent, this->evNeedsRepaintEvent);
  AddEventListener(this->evCursorEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    kneeboard->evSettingsChangedEvent,
    std::bind_front(&KneeboardView::UpdateUILayers, this));

  const auto id = this->GetRuntimeID().GetTemporaryValue();
  dprintf("Created kneeboard view ID {:#016x} ({})", id, id);
}

std::tuple<IUILayer*, std::span<IUILayer*>> KneeboardView::GetUILayers() const {
  std::span span(const_cast<KneeboardView*>(this)->mUILayers);
  return std::make_tuple(span.front(), span.subspan(1));
}

std::shared_ptr<KneeboardView> KneeboardView::Create(
  const DXResources& dxr,
  KneeboardState* kneeboard) {
  return std::shared_ptr<KneeboardView>(new KneeboardView(dxr, kneeboard));
}

void KneeboardView::UpdateUILayers() {
  decltype(mUILayers) layers;

  const auto settings = mKneeboard->GetAppSettings().mInGameUI;
  if (settings.mBookmarksBarEnabled) {
    layers.push_back(mBookmarksUILayer.get());
  }
  if (settings.mHeaderEnabled) {
    layers.push_back(mHeaderUILayer.get());
  }
  if (settings.mFooterEnabled) {
    layers.push_back(mFooterUILayer.get());
  }

  layers.push_back(mTabViewUILayer.get());
  mUILayers = layers;
}

KneeboardView::~KneeboardView() {
  this->RemoveAllEventListeners();
}

KneeboardViewID KneeboardView::GetRuntimeID() const {
  return mID;
}

void KneeboardView::SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs) {
  if (std::ranges::equal(tabs, mTabViews, {}, {}, &ITabView::GetTab)) {
    return;
  }

  decltype(mTabViews) tabViews;

  for (const auto& event: mTabEvents) {
    this->RemoveEventListener(event);
  }
  mTabEvents.clear();

  for (const auto& tab: tabs) {
    auto it = std::ranges::find(mTabViews, tab, &ITabView::GetTab);
    const auto tabView = (it != mTabViews.end())
      ? (*it)
      : std::make_shared<TabView>(mDXR, mKneeboard, tab);
    tabViews.push_back(tabView);

    mTabEvents.insert(
      mTabEvents.end(),
      {
        AddEventListener(
          tabView->evNeedsRepaintEvent,
          weak_wrap(tabView, this)([](auto tabView, auto self) {
            if (tabView == self->GetCurrentTabView()) {
              self->evNeedsRepaintEvent.Emit();
            }
          })),
        AddEventListener(
          tab->evAvailableFeaturesChangedEvent,
          weak_wrap(tabView, this)([](auto tabView, auto self) {
            if (tabView == self->GetCurrentTabView()) {
              self->evNeedsRepaintEvent.Emit();
            }
          })),
        AddEventListener(
          tabView->evBookmarksChangedEvent, this->evBookmarksChangedEvent),
      });
  }

  mTabViews = std::move(tabViews);
  auto it = std::ranges::find(mTabViews, mCurrentTabView);
  if (it == mTabViews.end()) {
    mCurrentTabView = tabs.empty() ? nullptr : mTabViews.front();
    this->evCurrentTabChangedEvent.Emit(this->GetTabIndex());
  }
}

TabIndex KneeboardView::GetTabIndex() const {
  auto it = std::ranges::find(mTabViews, mCurrentTabView);
  if (it == mTabViews.end()) {
    return 0;
  }
  return static_cast<TabIndex>(it - mTabViews.begin());
}

void KneeboardView::SetCurrentTabByIndex(TabIndex index) {
  if (index >= mTabViews.size()) {
    return;
  }
  if (mCurrentTabView == mTabViews.at(index)) {
    return;
  }
  if (mCurrentTabView) {
    mCurrentTabView->PostCursorEvent({});
  }
  mCurrentTabView = mTabViews.at(index);
  evCurrentTabChangedEvent.Emit(index);
}

void KneeboardView::SetCurrentTabByRuntimeID(ITab::RuntimeID id) {
  const auto it = std::ranges::find(mTabViews, id, [](const auto& view) {
    return view->GetRootTab()->GetRuntimeID();
  });

  if (it == mTabViews.end()) {
    return;
  }
  if (*it == mCurrentTabView) {
    return;
  }

  SetCurrentTabByIndex(it - mTabViews.begin());
}

void KneeboardView::PreviousTab() {
  const auto count = mTabViews.size();
  if (count < 2) {
    return;
  }

  const auto current = GetTabIndex();
  if (current > 0) {
    SetCurrentTabByIndex(current - 1);
    return;
  }

  if (mKneeboard->GetAppSettings().mLoopTabs) {
    SetCurrentTabByIndex(count - 1);
  }
}

void KneeboardView::NextTab() {
  const auto count = mTabViews.size();
  if (count < 2) {
    return;
  }

  const auto current = GetTabIndex();
  if (current + 1 < count) {
    SetCurrentTabByIndex(current + 1);
    return;
  }

  if (mKneeboard->GetAppSettings().mLoopTabs) {
    SetCurrentTabByIndex(0);
    return;
  }
}

std::shared_ptr<ITabView> KneeboardView::GetTabViewByID(
  ITab::RuntimeID id) const {
  for (const auto& tabView: mTabViews) {
    if (tabView->GetTab()->GetRuntimeID() == id) {
      return tabView;
    }
    if (tabView->GetRootTab()->GetRuntimeID() == id) {
      return tabView;
    }
  }
  dprint("Failed to find tab by ID");
  OPENKNEEBOARD_BREAK;
  return {};
}

std::shared_ptr<ITab> KneeboardView::GetCurrentTab() const {
  if (!mCurrentTabView) [[unlikely]] {
    return {};
  }
  return mCurrentTabView->GetTab();
}

std::shared_ptr<ITabView> KneeboardView::GetCurrentTabView() const {
  return mCurrentTabView;
}

D2D1_SIZE_U KneeboardView::GetCanvasSize() const {
  if (!mCurrentTabView) {
    return {ErrorRenderWidth, ErrorRenderHeight};
  }

  auto [first, rest] = this->GetUILayers();
  const auto metrics = first->GetMetrics(
    rest,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView
      = std::const_pointer_cast<KneeboardView>(this->shared_from_this()),
      .mIsActiveForInput = false,
    });
  const auto idealSize = metrics.mCanvasSize;
  if (metrics.mScalingKind == ScalingKind::Bitmap) {
    return {
      static_cast<UINT>(std::lround(idealSize.width)),
      static_cast<UINT>(std::lround(idealSize.height)),
    };
  }

  const auto xScale = TextureWidth / idealSize.width;
  const auto yScale = TextureHeight / idealSize.height;
  const auto scale = std::min(xScale, yScale);
  const D2D1_SIZE_U canvas {
    static_cast<UINT>(std::lround(idealSize.width * scale)),
    static_cast<UINT>(std::lround(idealSize.height * scale)),
  };
  return canvas;
}

D2D1_SIZE_U KneeboardView::GetContentNativeSize() const {
  if (!mCurrentTabView) {
    return {ErrorRenderWidth, ErrorRenderHeight};
  }
  return mCurrentTabView->GetNativeContentSize();
}

void KneeboardView::PostCursorEvent(const CursorEvent& ev) {
  if (winrt::apartment_context() != mUIThread) {
    dprint("Cursor event in wrong thread!");
    OPENKNEEBOARD_BREAK;
  }
  if (!mCurrentTabView) {
    return;
  }

  if (ev.mTouchState == CursorTouchState::NOT_NEAR_SURFACE) {
    mCursorCanvasPoint.reset();
  } else {
    mCursorCanvasPoint = {ev.mX, ev.mY};
  }

  auto [first, rest] = this->GetUILayers();
  first->PostCursorEvent(
    rest,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView = this->shared_from_this(),
      .mIsActiveForInput = true,
    },
    mEventContext,
    ev);

  evCursorEvent.Emit(ev);
}

void KneeboardView::RenderWithChrome(
  RenderTargetID rtid,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect,
  bool isActiveForInput) noexcept {
  if (!mCurrentTabView) {
    d2d->FillRectangle(rect, mErrorBackgroundBrush.get());
    mErrorRenderer->Render(d2d, _("No Tabs"), rect);
    return;
  }

  auto [first, rest] = this->GetUILayers();
  first->Render(
    rtid,
    rest,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView = this->shared_from_this(),
      .mIsActiveForInput = isActiveForInput,
    },
    d2d,
    rect);
  if (mCursorCanvasPoint) {
    const D2D1_SIZE_F size {
      rect.right - rect.left,
      rect.bottom - rect.top,
    };
    mCursorRenderer->Render(
      d2d,
      {
        mCursorCanvasPoint->x * size.width,
        mCursorCanvasPoint->y * size.height,
      },
      size);
  }
}

void KneeboardView::PostUserAction(UserAction action) {
  switch (action) {
    case UserAction::PREVIOUS_TAB:
      this->PreviousTab();
      return;
    case UserAction::NEXT_TAB:
      this->NextTab();
      return;
      return;
    case UserAction::PREVIOUS_BOOKMARK:
      this->GoToPreviousBookmark();
      return;
    case UserAction::NEXT_BOOKMARK:
      this->GoToNextBookmark();
      return;
    case UserAction::NEXT_PAGE:
    case UserAction::TOGGLE_BOOKMARK:
    case UserAction::PREVIOUS_PAGE:
    case UserAction::RELOAD_CURRENT_TAB:
      if (
        auto handler = UserActionHandler::Create(
          mKneeboard, shared_from_this(), this->GetCurrentTabView(), action)) {
        handler->Execute();
        return;
      }
      dprintf(
        "No KneeboardView action handler for action {}",
        static_cast<int>(action));
      OPENKNEEBOARD_BREAK;
      return;
    case UserAction::TOGGLE_VISIBILITY:
    case UserAction::TOGGLE_FORCE_ZOOM:
    case UserAction::RECENTER_VR:
      // Handled by KneeboardState
      return;
  }
  OPENKNEEBOARD_BREAK;
}

std::optional<D2D1_POINT_2F> KneeboardView::GetCursorCanvasPoint() const {
  return mCursorCanvasPoint;
}

std::optional<D2D1_POINT_2F> KneeboardView::GetCursorContentPoint() const {
  return mTabViewUILayer->GetCursorPoint();
}

D2D1_POINT_2F KneeboardView::GetCursorCanvasPoint(
  const D2D1_POINT_2F& contentPoint) const {
  auto [first, rest] = this->GetUILayers();
  const auto mapping = first->GetMetrics(
    rest,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView = std::static_pointer_cast<IKneeboardView>(
        std::const_pointer_cast<KneeboardView>(this->shared_from_this())),
      .mIsActiveForInput = true,
    });

  const auto& contentArea = mapping.mContentArea;
  const D2D1_SIZE_F contentSize {
    contentArea.right - contentArea.left,
    contentArea.bottom - contentArea.top,
  };
  const auto& canvasSize = mapping.mCanvasSize;

  D2D1_POINT_2F point {contentPoint};
  point.x *= contentSize.width;
  point.y *= contentSize.height;
  point.x += contentArea.left;
  point.y += contentArea.top;

  return {
    point.x / canvasSize.width,
    point.y / canvasSize.height,
  };
}

bool KneeboardView::CurrentPageHasBookmark() const {
  auto view = this->GetCurrentTabView();
  if (!view) {
    return false;
  }
  auto tab = view->GetRootTab();
  auto page = view->GetPageID();

  auto bookmarks = tab->GetBookmarks();
  for (const auto& bookmark: tab->GetBookmarks()) {
    if (bookmark.mTabID == tab->GetRuntimeID() && bookmark.mPageID == page) {
      return true;
    }
  }

  return false;
}

void KneeboardView::RemoveBookmarkForCurrentPage() {
  auto view = this->GetCurrentTabView();
  auto tab = view->GetRootTab();
  auto page = view->GetPageID();

  auto bookmarks = tab->GetBookmarks();
  auto it = std::ranges::find_if(bookmarks, [=](const auto& bm) {
    return bm.mTabID == tab->GetRuntimeID() && bm.mPageID == page;
  });

  if (it == bookmarks.end()) {
    return;
  }

  bookmarks.erase(it);
  tab->SetBookmarks(bookmarks);
}

std::optional<Bookmark> KneeboardView::AddBookmarkForCurrentPage() {
  EventDelay delay;
  std::unique_lock lock(*mKneeboard);

  auto view = this->GetCurrentTabView();
  if (view->GetTabMode() != TabMode::NORMAL) {
    OPENKNEEBOARD_BREAK;
    return {};
  }

  auto tab = view->GetRootTab();

  Bookmark ret {
    .mTabID = tab->GetRuntimeID(),
    .mPageID = view->GetPageID(),
  };

  const auto pageIDs = view->GetPageIDs();
  const auto pageIt = std::ranges::find(pageIDs, ret.mPageID);
  if (pageIt == pageIDs.end()) {
    // Should be impossible - current page ID must be in page IDs set
    OPENKNEEBOARD_BREAK;
    return ret;
  }
  const auto pageIndex = pageIt - pageIDs.begin();

  bool inserted = false;
  auto bookmarks = tab->GetBookmarks();
  for (auto it = bookmarks.begin(); it != bookmarks.end();) {
    const auto bookmarkIt = std::ranges::find(pageIDs, it->mPageID);
    if (bookmarkIt == pageIDs.end()) {
      it = bookmarks.erase(it);
      continue;
    }
    const auto bookmarkIndex = bookmarkIt - pageIDs.begin();
    if (bookmarkIndex > pageIndex) {
      bookmarks.insert(it, ret);
      inserted = true;
      break;
    }
    ++it;
  }
  if (!inserted) {
    bookmarks.push_back(ret);
  }

  tab->SetBookmarks(bookmarks);

  return ret;
}

std::vector<Bookmark> KneeboardView::GetBookmarks() const {
  std::vector<Bookmark> ret;
  auto inserter = std::back_inserter(ret);
  for (const auto& tv: mTabViews) {
    std::ranges::copy(tv->GetRootTab()->GetBookmarks(), inserter);
  }
  return ret;
}

void KneeboardView::RemoveBookmark(const Bookmark& bookmark) {
  EventDelay delay;
  std::unique_lock lock(*mKneeboard);

  auto tabIt = std::ranges::find_if(mTabViews, [&](const auto& tv) {
    return tv->GetRootTab()->GetRuntimeID() == bookmark.mTabID;
  });
  if (tabIt == mTabViews.end()) {
    return;
  }
  auto tab = (*tabIt)->GetRootTab();

  auto bookmarks = tab->GetBookmarks();
  auto it = std::ranges::find(bookmarks, bookmark);
  if (it == bookmarks.end()) {
    return;
  }
  bookmarks.erase(it);
  tab->SetBookmarks(bookmarks);
}

void KneeboardView::GoToBookmark(const Bookmark& bookmark) {
  const auto it = std::ranges::find(
    mTabViews, bookmark.mTabID, [](const auto& view) noexcept {
      return view->GetRootTab()->GetRuntimeID();
    });

  if (it == mTabViews.end()) {
    return;
  }
  if (GetCurrentTabView() != *it) {
    SetCurrentTabByIndex(it - mTabViews.begin());
  }
  (*it)->SetTabMode(TabMode::NORMAL);
  (*it)->SetPageID(bookmark.mPageID);
}

void KneeboardView::GoToPreviousBookmark() {
  this->SetBookmark(RelativePosition::Previous);
}

void KneeboardView::GoToNextBookmark() {
  this->SetBookmark(RelativePosition::Next);
}

void KneeboardView::SetBookmark(RelativePosition pos) {
  auto bookmark = this->GetBookmark(pos);
  if (bookmark) {
    GoToBookmark(*bookmark);
    return;
  }

  if (!mKneeboard->GetAppSettings().mBookmarks.mLoop) {
    return;
  }

  const auto bookmarks = this->GetBookmarks();
  if (bookmarks.empty()) {
    return;
  }

  if (pos == RelativePosition::Previous) {
    GoToBookmark(bookmarks.back());
  } else {
    GoToBookmark(bookmarks.front());
  }
}

std::optional<Bookmark> KneeboardView::GetBookmark(RelativePosition pos) const {
  const auto currentTabID = mCurrentTabView->GetRootTab()->GetRuntimeID();
  const auto pages = mCurrentTabView->GetPageIDs();
  const auto currentPageIt
    = std::ranges::find(pages, mCurrentTabView->GetPageID());
  if (currentPageIt == pages.end()) {
    return {};
  }

  std::optional<Bookmark> prev;
  bool reachedCurrentTab = false;
  for (const auto& bookmark: this->GetBookmarks()) {
    const bool isCurrentTab = bookmark.mTabID == currentTabID;
    if (reachedCurrentTab && !isCurrentTab) {
      switch (pos) {
        case RelativePosition::Previous:
          return prev;
        case RelativePosition::Next:
          return bookmark;
      }
      return {};
    }

    if (!(isCurrentTab || reachedCurrentTab)) {
      prev = bookmark;
      continue;
    }

    if (!isCurrentTab) {
      throw std::logic_error("Should be current tab");
    }
    reachedCurrentTab = true;

    const auto bookmarkPageIt = std::ranges::find(pages, bookmark.mPageID);
    if (bookmarkPageIt == pages.end()) {
      continue;
    }

    if (bookmarkPageIt < currentPageIt) {
      prev = bookmark;
      continue;
    }
    if (bookmarkPageIt == currentPageIt) {
      continue;
    }

    if (pos == RelativePosition::Previous) {
      return prev;
    }
    return bookmark;
  }

  if (pos == RelativePosition::Previous) {
    return prev;
  }
  return {};
}

}// namespace OpenKneeboard
