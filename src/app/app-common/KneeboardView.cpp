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
#include <OpenKneeboard/SHM/ActiveConsumers.h>
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

KneeboardView::KneeboardView(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const winrt::guid& guid)
  : mDXR(dxr), mKneeboard(kneeboard), mGUID(guid) {
  mCursorRenderer = std::make_unique<CursorRenderer>(dxr);
  mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr);

  dxr->mD2DDeviceContext->CreateSolidColorBrush(
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
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const winrt::guid& guid) {
  return std::shared_ptr<KneeboardView>(
    new KneeboardView(dxr, kneeboard, guid));
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

winrt::guid KneeboardView::GetPersistentGUID() const {
  return mGUID;
}

KneeboardViewID KneeboardView::GetRuntimeID() const {
  return mID;
}

void KneeboardView::SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs) {
  if (std::ranges::equal(tabs, mTabViews, {}, {}, &ITabView::GetTab)) {
    return;
  }

  decltype(mTabViews) tabViews;

  for (const auto& tab: tabs) {
    auto it = std::ranges::find(mTabViews, tab, &ITabView::GetTab);
    const auto tabView = (it != mTabViews.end())
      ? (*it)
      : std::make_shared<TabView>(mDXR, mKneeboard, tab);
    tabViews.push_back(tabView);
  }

  auto it = std::ranges::find(tabViews, mCurrentTabView);
  if (it == tabViews.end()) {
    SetTabViews(std::move(tabViews), tabs.empty() ? nullptr : tabViews.front());
  } else {
    SetTabViews(std::move(tabViews), mCurrentTabView);
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

KneeboardView::IPCRenderLayout KneeboardView::GetIPCRenderLayout() const {
  if (!mCurrentTabView) {
    return {};
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

  const auto idealSize = metrics.mPreferredSize.mPixelSize;
  const auto unscaledContentArea = metrics.mContentArea;
  if (metrics.mPreferredSize.mScalingKind == ScalingKind::Bitmap) {
    if (
      idealSize.mWidth <= MaxViewRenderSize.mWidth
      && idealSize.mHeight <= MaxViewRenderSize.mHeight) {
      return {
        idealSize,
        unscaledContentArea,
      };
    }
    const auto scaleX = MaxViewRenderSize.Width<float>() / idealSize.mWidth;
    const auto scaleY = MaxViewRenderSize.Height<float>() / idealSize.mHeight;
    const auto scale = std::min(scaleX, scaleY);

    // Integer scaling gets us the most readable results
    const auto divisor = static_cast<uint32_t>(std::ceil(1 / scale));
    return {
      idealSize / divisor,
      (unscaledContentArea / divisor),
    };
  }

  const auto consumers = SHM::ActiveConsumers::Get();
  if (consumers.Any() == SHM::ActiveConsumers::T {}) {
    const auto size = idealSize.ScaledToFit(MaxViewRenderSize);
    const auto ratio = static_cast<float>(size.mWidth) / idealSize.mWidth;

    return {
      size,
      (unscaledContentArea.StaticCast<float>() * ratio).Rounded<uint32_t>(),
    };
  }
  const auto now = SHM::ActiveConsumers::Clock::now();

  const bool haveVR = (now - consumers.AnyVR()) < std::chrono::seconds(1);

  if (haveVR) {
    const auto size = idealSize.ScaledToFit(MaxViewRenderSize);
    const auto ratio = static_cast<float>(size.mWidth) / idealSize.mWidth;
    return {
      size,
      (unscaledContentArea.StaticCast<float>() * ratio).Rounded<uint32_t>(),
    };
  }

  const auto& consumerSize = consumers.mNonVRPixelSize;
  const auto haveConsumerSize = consumerSize != PixelSize {};
  const bool haveNonVR = haveConsumerSize
    && (now - consumers.NotVROrViewer()) < std::chrono::milliseconds(500);
  const bool haveViewer = haveConsumerSize
    && (now - consumers.mViewer) < std::chrono::milliseconds(500);
  if (haveNonVR) {
    const auto views = mKneeboard->GetViewsSettings().mViews;
    const auto view = std::ranges::find(views, mGUID, &ViewConfig::mGuid);
    if (view != views.end()) [[likely]] {
      const auto pos = view->mNonVR.Resolve(
        metrics.mPreferredSize,
        PixelRect {{0, 0}, metrics.mPreferredSize.mPixelSize},
        unscaledContentArea,
        views);
      if (pos) {
        const auto rect = pos->mPosition.Layout(consumerSize, idealSize);
        const auto ratio
          = static_cast<float>(rect.mSize.mWidth) / idealSize.mWidth;
        return {
          rect.mSize,
          (unscaledContentArea.StaticCast<float>() * ratio).Rounded<uint32_t>(),
        };
      }
    } else {
      traceprint("View with invalid GUID");
      OPENKNEEBOARD_BREAK;
    }
  }

  const auto size
    = idealSize.ScaledToFit(haveViewer ? consumerSize : MaxViewRenderSize);
  const auto ratio = static_cast<float>(size.mWidth) / idealSize.mWidth;
  return {
    size,
    (unscaledContentArea.StaticCast<float>() * ratio).Rounded<uint32_t>(),
  };
}

PreferredSize KneeboardView::GetPreferredSize() const {
  if (!mCurrentTabView) {
    return {ErrorRenderSize, ScalingKind::Vector};
  }
  return mCurrentTabView->GetPreferredSize();
}

void KneeboardView::PostCursorEvent(const CursorEvent& ev) {
  mThreadGuard.CheckThread();
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
  RenderTarget* rt,
  const PixelRect& rect,
  bool isActiveForInput) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("KneeboardView::RenderWithChrome()");
  if (!mCurrentTabView) {
    auto d2d = rt->d2d();
    d2d->FillRectangle(rect, mErrorBackgroundBrush.get());
    mErrorRenderer->Render(d2d, _("No Tabs"), rect);
    return;
  }

  auto [first, rest] = this->GetUILayers();
  {
    OPENKNEEBOARD_TraceLoggingScope("RenderWithChrome/RenderUILayers");
    first->Render(
      rt,
      rest,
      {
        .mTabView = mCurrentTabView,
        .mKneeboardView = this->shared_from_this(),
        .mIsActiveForInput = isActiveForInput,
      },
      rect);
  }
  if (mCursorCanvasPoint) {
    const auto& size = rect.mSize;
    auto d2d = rt->d2d();
    mCursorRenderer->Render(
      d2d,
      {
        (mCursorCanvasPoint->x * size.mWidth) + rect.Left(),
        (mCursorCanvasPoint->y * size.mHeight) + rect.Top(),
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
      .mKneeboardView = std::static_pointer_cast<KneeboardView>(
        std::const_pointer_cast<KneeboardView>(this->shared_from_this())),
      .mIsActiveForInput = true,
    });

  const auto& contentArea = mapping.mContentArea;
  const auto contentSize = contentArea.mSize;
  const auto& canvasSize
    = mapping.mPreferredSize.mPixelSize.StaticCast<float>();

  D2D1_POINT_2F point {contentPoint};
  point.x *= contentSize.Width();
  point.y *= contentSize.Height();
  point.x += contentArea.Left();
  point.y += contentArea.Top();

  return {
    point.x / canvasSize.mWidth,
    point.y / canvasSize.mHeight,
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

void KneeboardView::SwapState(KneeboardView& other) {
  auto otherViews = other.mTabViews;
  auto otherCurrent = other.mCurrentTabView;
  other.SetTabViews(std::move(mTabViews), mCurrentTabView);
  mTabViews = {};
  mCurrentTabView = {};
  this->SetTabViews(std::move(otherViews), otherCurrent);
}

void KneeboardView::SetTabViews(
  std::vector<std::shared_ptr<ITabView>>&& views,
  const std::shared_ptr<ITabView>& currentView) {
  mTabViews = std::move(views);

  for (const auto& event: mTabEvents) {
    this->RemoveEventListener(event);
  }
  mTabEvents.clear();

  for (const auto& tabView: mTabViews) {
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
          tabView->evBookmarksChangedEvent, this->evBookmarksChangedEvent),
        AddEventListener(
          tabView->GetRootTab()->evAvailableFeaturesChangedEvent,
          weak_wrap(tabView, this)([](auto tabView, auto self) {
            if (tabView == self->GetCurrentTabView()) {
              self->evNeedsRepaintEvent.Emit();
            }
          })),
      });
  }

  if (currentView != mCurrentTabView) {
    mCurrentTabView = currentView;
    evCurrentTabChangedEvent.Emit(this->GetTabIndex());
  }
}

}// namespace OpenKneeboard
