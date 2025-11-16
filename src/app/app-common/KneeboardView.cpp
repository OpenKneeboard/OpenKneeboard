// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/BookmarksUILayer.hpp>
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/CursorRenderer.hpp>
#include <OpenKneeboard/D2DErrorRenderer.hpp>
#include <OpenKneeboard/FooterUILayer.hpp>
#include <OpenKneeboard/HeaderUILayer.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/PluginTab.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabViewUILayer.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/UserAction.hpp>
#include <OpenKneeboard/UserActionHandler.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>

#include <algorithm>
#include <ranges>

namespace OpenKneeboard {

KneeboardView::KneeboardView(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const winrt::guid& guid,
  std::string_view name)
  : mDXR(dxr), mKneeboard(kneeboard), mGuid(guid), mName(name) {
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
  dprint("Created kneeboard view ID {:#018x} ({})", id, id);
}

std::tuple<IUILayer*, std::span<IUILayer*>> KneeboardView::GetUILayers() const {
  std::span span(const_cast<KneeboardView*>(this)->mUILayers);
  return std::make_tuple(span.front(), span.subspan(1));
}

std::shared_ptr<KneeboardView> KneeboardView::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const winrt::guid& guid,
  std::string_view name) {
  return std::shared_ptr<KneeboardView>(
    new KneeboardView(dxr, kneeboard, guid, name));
}

void KneeboardView::UpdateUILayers() {
  decltype(mUILayers) layers;

  const auto settings = mKneeboard->GetUISettings().mInGameUI;
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
  mThreadGuard.CheckThread();
  this->RemoveAllEventListeners();
}

winrt::guid KneeboardView::GetPersistentGUID() const noexcept {
  return mGuid;
}

KneeboardViewID KneeboardView::GetRuntimeID() const noexcept {
  return mRuntimeID;
}

std::string_view KneeboardView::GetName() const noexcept {
  return mName;
}

void KneeboardView::SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs) {
  mThreadGuard.CheckThread();

  if (tabs == this->GetRootTabs()) {
    return;
  }

  decltype(mTabViews) tabViews;

  for (const auto& tab: tabs) {
    auto it = std::ranges::find_if(
      mTabViews, [&tab](const std::shared_ptr<TabView>& tv) {
        return tab == tv->GetTab().lock();
      });
    const auto tabView = (it != mTabViews.end())
      ? (*it)
      : std::make_shared<TabView>(mDXR, mKneeboard, tab, mRuntimeID);
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

void KneeboardView::SetCurrentTabByIndex(
  TabIndex index,
  const std::source_location& loc) {
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

  auto tab = mCurrentTabView->GetRootTab().lock();
  if (tab) {
    dprint(
      "Current tab for kneeboard view {:#018x} changed to '{}' ({}) @ {}",
      this->GetRuntimeID().GetTemporaryValue(),
      tab->GetTitle(),
      tab->GetPersistentID(),
      loc);
  } else {
    dprint.Warning("Switched to TabView with nullptr tab");
  }
  evCurrentTabChangedEvent.Emit(index);
}

void KneeboardView::SetCurrentTabByRuntimeID(
  ITab::RuntimeID id,
  const std::source_location& loc) {
  for (std::size_t i = 0; i < mTabViews.size(); ++i) {
    const auto tab = mTabViews.at(i)->GetRootTab().lock();
    if (!tab) {
      continue;
    }
    if (tab->GetRuntimeID() != id) {
      continue;
    }
    SetCurrentTabByIndex(i);
    return;
  }
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

  if (mKneeboard->GetUISettings().mLoopTabs) {
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

  if (mKneeboard->GetUISettings().mLoopTabs) {
    SetCurrentTabByIndex(0);
    return;
  }
}

std::shared_ptr<TabView> KneeboardView::GetTabViewByID(
  ITab::RuntimeID id) const {
  for (const auto& tabView: mTabViews) {
    const auto tab = tabView->GetTab().lock();
    if (!tab) {
      continue;
    }
    if (tab->GetRuntimeID() == id) {
      return tabView;
    }

    const auto rootTab = tabView->GetRootTab().lock();
    if (rootTab && rootTab->GetRuntimeID() == id) {
      return tabView;
    }
  }
  dprint("Failed to find tab by ID");
  OPENKNEEBOARD_BREAK;
  return {};
}

std::weak_ptr<ITab> KneeboardView::GetCurrentTab() const {
  if (!mCurrentTabView) [[unlikely]] {
    return {};
  }
  return mCurrentTabView->GetTab();
}

std::shared_ptr<TabView> KneeboardView::GetCurrentTabView() const {
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
      (unscaledContentArea.StaticCast<float>() * ratio)
        .Rounded<uint32_t>()
        .Clamped(MaxViewRenderSize),
    };
  }
  const auto now = SHM::ActiveConsumers::Clock::now();

  const bool haveVR = (now - consumers.AnyVR()) < std::chrono::seconds(1);

  if (haveVR) {
    const auto size = idealSize.ScaledToFit(MaxViewRenderSize);
    const auto ratio = static_cast<float>(size.mWidth) / idealSize.mWidth;
    return {
      size,
      (unscaledContentArea.StaticCast<float>() * ratio)
        .Rounded<uint32_t>()
        .Clamped(MaxViewRenderSize),
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
    const auto view = std::ranges::find(views, mGuid, &ViewSettings::mGuid);
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
          (unscaledContentArea.StaticCast<float>() * ratio)
            .Rounded<uint32_t>()
            .Clamped(MaxViewRenderSize),
        };
      }
    } else {
      TraceLoggingWrite(
        gTraceProvider,
        "KneeboardView::GetIPCRenderLayout()/ViewNotFound",
        TraceLoggingValue(to_string(to_hstring(mGuid)).c_str(), "GUID"));
      OPENKNEEBOARD_BREAK;
    }
  }

  const auto size
    = idealSize.ScaledToFit(haveViewer ? consumerSize : MaxViewRenderSize);
  const auto ratio = static_cast<float>(size.mWidth) / idealSize.mWidth;
  return {
    size,
    (unscaledContentArea.StaticCast<float>() * ratio)
      .Rounded<uint32_t>()
      .Clamped(MaxViewRenderSize),
  };
}

PreferredSize KneeboardView::GetPreferredSize() const {
  if (!mCurrentTabView) {
    return ErrorPreferredSize;
  }
  return mCurrentTabView->GetPreferredSize().value_or(ErrorPreferredSize);
}

void KneeboardView::PostCursorEvent(const CursorEvent& ev) {
  mThreadGuard.CheckThread();
  if (!mCurrentTabView) {
    return;
  }

  if (ev.mTouchState == CursorTouchState::NotNearSurface) {
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
    mRuntimeID,
    ev);

  evCursorEvent.Emit(ev);
}

task<void> KneeboardView::RenderWithChrome(
  RenderTarget* rt,
  const PixelRect& rect,
  bool isActiveForInput) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "KneeboardView::RenderWithChrome()",
    TraceLoggingHexUInt64(rt->GetID().GetTemporaryValue(), "RenderTargetID"));
  const RenderContext rc {rt, this};
  if (!mCurrentTabView) {
    auto d2d = rt->d2d();
    d2d->FillRectangle(rect, mErrorBackgroundBrush.get());
    mErrorRenderer->Render(d2d, _("No Tabs"), rect);
    co_return;
  }

  auto [first, rest] = this->GetUILayers();
  {
    OPENKNEEBOARD_TraceLoggingScope("RenderWithChrome/RenderUILayers");
    co_await first->Render(
      rc,
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
      Geometry2D::Point<float>(
        (mCursorCanvasPoint->x * size.mWidth) + rect.Left(),
        (mCursorCanvasPoint->y * size.mHeight) + rect.Top())
        .Rounded<uint32_t>(),
      size);
  }
}

task<void> KneeboardView::PostUserAction(UserAction action) {
  switch (action) {
    case UserAction::PREVIOUS_TAB: {
      this->PreviousTab();
      auto tab = this->GetCurrentTab().lock();
      if (tab) {
        dprint("Previous tab to '{}'", tab->GetTitle());
      }
      co_return;
    }
    case UserAction::NEXT_TAB: {
      this->NextTab();
      auto tab = this->GetCurrentTab().lock();
      if (tab) {
        dprint("Next tab to '{}'", tab->GetTitle());
      }
      co_return;
    }
    case UserAction::PREVIOUS_BOOKMARK:
      this->GoToPreviousBookmark();
      co_return;
    case UserAction::NEXT_BOOKMARK:
      this->GoToNextBookmark();
      co_return;
    case UserAction::NEXT_PAGE:
    case UserAction::TOGGLE_BOOKMARK:
    case UserAction::PREVIOUS_PAGE:
      if (
        auto handler = UserActionHandler::Create(
          mKneeboard, shared_from_this(), this->GetCurrentTabView(), action)) {
        co_await handler->Execute();
        co_return;
      }
      dprint(
        "No KneeboardView action handler for action {}",
        static_cast<int>(action));
      OPENKNEEBOARD_BREAK;
      co_return;
    case UserAction::CYCLE_ACTIVE_VIEW:
    case UserAction::DECREASE_BRIGHTNESS:
    case UserAction::DISABLE_TINT:
    case UserAction::ENABLE_TINT:
    case UserAction::HIDE:
    case UserAction::INCREASE_BRIGHTNESS:
    case UserAction::RECENTER_VR:
    case UserAction::REPAINT_NOW:
    case UserAction::SHOW:
    case UserAction::SWAP_FIRST_TWO_VIEWS:
    case UserAction::TOGGLE_FORCE_ZOOM:
    case UserAction::TOGGLE_TINT:
    case UserAction::TOGGLE_VISIBILITY:
    case UserAction::NEXT_PROFILE:
    case UserAction::PREVIOUS_PROFILE:
      // Handled by KneeboardState
      co_return;
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
  auto tab = view->GetRootTab().lock();
  if (!tab) {
    return false;
  }
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
  auto tab = view->GetRootTab().lock();
  auto page = view->GetPageID();

  if (!tab) {
    return;
  }

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
  if (view->GetTabMode() != TabMode::Normal) {
    OPENKNEEBOARD_BREAK;
    return {};
  }

  auto tab = view->GetRootTab().lock();
  if (!tab) {
    return {};
  }

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
  for (auto&& tab: this->GetRootTabs()) {
    std::ranges::copy(tab->GetBookmarks(), inserter);
  }
  return ret;
}

std::vector<std::shared_ptr<ITab>> KneeboardView::GetRootTabs() const {
  std::vector<std::shared_ptr<ITab>> ret;
  ret.reserve(mTabViews.size());
  for (auto&& view: mTabViews) {
    auto tab = view->GetRootTab().lock();
    if (tab) {
      ret.push_back(std::move(tab));
    }
  }
  return ret;
}

void KneeboardView::RemoveBookmark(const Bookmark& bookmark) {
  EventDelay delay;
  std::unique_lock lock(*mKneeboard);

  const auto tabs = this->GetRootTabs();
  auto tabIt = std::ranges::find_if(tabs, [&](const auto& tab) {
    return tab->GetRuntimeID() == bookmark.mTabID;
  });
  if (tabIt == tabs.end()) {
    return;
  }
  auto tab = *tabIt;

  auto bookmarks = tab->GetBookmarks();
  auto it = std::ranges::find(bookmarks, bookmark);
  if (it == bookmarks.end()) {
    return;
  }
  bookmarks.erase(it);
  tab->SetBookmarks(bookmarks);
}

void KneeboardView::GoToBookmark(const Bookmark& bookmark) {
  for (std::size_t i = 0; i < mTabViews.size(); ++i) {
    const auto& view = mTabViews.at(i);
    const auto tab = view->GetRootTab().lock();
    if (!tab) {
      continue;
    }
    if (tab->GetRuntimeID() != bookmark.mTabID) {
      continue;
    }
    if (GetCurrentTabView() != view) {
      SetCurrentTabByIndex(i);
    }
    view->SetTabMode(TabMode::Normal);
    view->SetPageID(bookmark.mPageID);
    return;
  }
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

  if (!mKneeboard->GetUISettings().mBookmarks.mLoop) {
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
  const auto tab = mCurrentTabView->GetRootTab().lock();
  if (!tab) {
    return {};
  }
  const auto currentTabID = tab->GetRuntimeID();
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
  mThreadGuard.CheckThread();
  auto otherViews = other.mTabViews;
  auto otherCurrent = other.mCurrentTabView;
  other.SetTabViews(std::move(mTabViews), mCurrentTabView);
  mTabViews = {};
  mCurrentTabView = {};
  this->SetTabViews(std::move(otherViews), otherCurrent);
}

void KneeboardView::SetTabViews(
  std::vector<std::shared_ptr<TabView>>&& views,
  const std::shared_ptr<TabView>& currentView) {
  mThreadGuard.CheckThread();
  mTabViews = std::move(views);

  for (const auto& event: mTabEvents) {
    this->RemoveEventListener(event);
  }
  mTabEvents.clear();

  for (const auto& tabView: mTabViews) {
    const auto tab = tabView->GetRootTab().lock();
    if (!tab) {
      continue;
    }

    auto repaint = [](auto self, auto tabView) {
      if (self->GetCurrentTabView() != tabView) {
        return;
      }
      self->evNeedsRepaintEvent.Emit();
    } | bind_refs_front(this, tabView);

    mTabEvents.insert(
      mTabEvents.end(),
      {
        AddEventListener(tabView->evNeedsRepaintEvent, repaint),
        AddEventListener(
          tabView->evBookmarksChangedEvent, this->evBookmarksChangedEvent),
        AddEventListener(tab->evAvailableFeaturesChangedEvent, repaint),
      });
  }

  if (currentView != mCurrentTabView) {
    mCurrentTabView = currentView;
    evCurrentTabChangedEvent.Emit(this->GetTabIndex());
  }
}

void KneeboardView::PostCustomAction(
  std::string_view id,
  const nlohmann::json& arg) {
  const auto it = std::dynamic_pointer_cast<PluginTab>(
    mCurrentTabView->GetRootTab().lock());
  if (!it) {
    return;
  }
  it->PostCustomAction(mRuntimeID, id, arg);
}

std::vector<winrt::guid> KneeboardView::GetTabIDs() const noexcept {
  mThreadGuard.CheckThread();
  std::vector<winrt::guid> ret;
  for (auto&& tab: this->GetRootTabs()) {
    ret.push_back(tab->GetPersistentID());
  }
  return ret;
}

}// namespace OpenKneeboard
