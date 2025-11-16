// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/CachedLayer.hpp>
#include <OpenKneeboard/DoodleRenderer.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <OpenKneeboard/bindline.hpp>
#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <algorithm>
#include <numeric>

namespace OpenKneeboard {

PageSourceWithDelegates::PageSourceWithDelegates(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : mDXResources(dxr) {
  mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);
  mFixedEvents = {
    AddEventListener(mDoodles->evNeedsRepaintEvent, this->evNeedsRepaintEvent),
    AddEventListener(
      mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent),
    AddEventListener(
      this->evContentChangedEvent,
      [this]() {
        this->mContentLayerCache.clear();
        std::unordered_set<PageID> keep;
        for (const auto pageID: this->GetPageIDs()) {
          keep.insert(pageID);
        }
        this->mDoodles->ClearExcept(keep);
      }),
  };
}

PageSourceWithDelegates::~PageSourceWithDelegates() {
  OPENKNEEBOARD_TraceLoggingScope(
    "PageSourceWithDelegates::~PageSourceWithDelegates()");
  for (auto& event: mDelegateEvents) {
    this->RemoveEventListener(event);
  }
  for (auto& event: mFixedEvents) {
    this->RemoveEventListener(event);
  }
}

task<void> PageSourceWithDelegates::DisposeAsync() noexcept {
  OPENKNEEBOARD_TraceLoggingCoro("PageSourceWithDelegates::DisposeAsync()");
  const auto disposing = co_await mDisposal.StartOnce();
  if (!disposing) {
    co_return;
  }

  co_await this->SetDelegates({});
}

task<void> PageSourceWithDelegates::SetDelegates(
  std::vector<std::shared_ptr<IPageSource>> delegates) {
  winrt::apartment_context thread;
  OPENKNEEBOARD_TraceLoggingCoro("PageSourceWithDelegates::SetDelegates()");

  auto keepAlive = shared_from_this();

  auto disposers = mDelegates | std::views::transform([](auto it) {
                     return std::dynamic_pointer_cast<IHasDisposeAsync>(it);
                   })
    | std::views::filter([](auto it) -> bool { return !!it; })
    | std::views::transform([](auto it) { return it->DisposeAsync(); })
    | std::ranges::to<std::vector>();
  for (auto& it: disposers) {
    co_await std::move(it);
  }

  mPageDelegates.clear();
  co_await thread;

  for (auto& event: mDelegateEvents) {
    this->RemoveEventListener(event);
  }
  mDelegateEvents.clear();
  mDelegates = delegates;

  for (auto& delegate: delegates) {
    std::ranges::copy(
      std::vector<EventHandlerToken> {
        AddEventListener(
          delegate->evNeedsRepaintEvent, this->evNeedsRepaintEvent),
        AddEventListener(
          delegate->evPageAppendedEvent, this->evPageAppendedEvent),
        AddEventListener(
          delegate->evContentChangedEvent, this->evContentChangedEvent),
        AddEventListener(
          delegate->evAvailableFeaturesChangedEvent,
          this->evAvailableFeaturesChangedEvent),
        AddEventListener(
          delegate->evPageChangeRequestedEvent,
          this->evPageChangeRequestedEvent),
      },
      std::back_inserter(mDelegateEvents));
  }

  this->evContentChangedEvent.Emit();
}

PageIndex PageSourceWithDelegates::GetPageCount() const {
  PageIndex count = 0;
  for (const auto& delegate: mDelegates) {
    count += delegate->GetPageCount();
  }
  return count;
}

std::vector<PageID> PageSourceWithDelegates::GetPageIDs() const {
  std::vector<PageID> ret;
  for (const auto& delegate: mDelegates) {
    auto ids = delegate->GetPageIDs();
    ret.insert(ret.end(), ids.begin(), ids.end());
  }
  return ret;
}

std::shared_ptr<IPageSource> PageSourceWithDelegates::FindDelegate(
  PageID pageID) const {
  if (!pageID) {
    return {nullptr};
  }
  if (mPageDelegates.contains(pageID)) {
    return mPageDelegates.at(pageID).lock();
  }

  auto delegate
    = std::ranges::find_if(mDelegates, [pageID](const auto& delegate) {
        auto pageIDs = delegate->GetPageIDs();
        auto it = std::ranges::find(pageIDs, pageID);
        return it != pageIDs.end();
      });
  if (delegate == mDelegates.end()) {
    return {nullptr};
  }
  mPageDelegates[pageID] = *delegate;
  return *delegate;
}

std::optional<PreferredSize> PageSourceWithDelegates::GetPreferredSize(
  PageID pageID) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    return std::nullopt;
  }
  return delegate->GetPreferredSize(pageID);
}

task<void> PageSourceWithDelegates::RenderPage(
  RenderContext rc,
  PageID pageID,
  PixelRect rect) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    // Expected e.g. for a window capture tab with no matching window
    co_return;
  }

  auto withInternalCaching
    = std::dynamic_pointer_cast<IPageSourceWithInternalCaching>(delegate);
  if (withInternalCaching) {
    co_await delegate->RenderPage(rc, pageID, rect);
  } else {
    co_await this->RenderPageWithCache(
      delegate.get(), rc.GetRenderTarget(), pageID, rect);
  }

  auto withCursorEvents
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);

  if (!withCursorEvents) {
    mDoodles->Render(rc.GetRenderTarget(), pageID, rect);
  }
}

task<void> PageSourceWithDelegates::RenderPageWithCache(
  IPageSource* delegate,
  RenderTarget* rt,
  PageID pageID,
  PixelRect rect) {
  const auto rtid = rt->GetID();
  if (!mContentLayerCache.contains(rtid)) {
    mContentLayerCache[rtid] = std::make_unique<CachedLayer>(mDXResources);
  }

  co_await mContentLayerCache[rtid]->Render(
    rect,
    pageID.GetTemporaryValue(),
    rt,
    [](auto delegate, auto pageID, RenderTarget* rt, PixelSize size)
      -> task<void> {
      co_await delegate->RenderPage(
        RenderContext {rt, nullptr}, pageID, {{0, 0}, size});
    } | bind_front(delegate, pageID));
}

bool PageSourceWithDelegates::CanClearUserInput() const {
  if (mDoodles->HaveDoodles()) {
    return true;
  }
  for (const auto& delegate: mDelegates) {
    auto wce = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
    if (wce && wce->CanClearUserInput()) {
      return true;
    }
  }
  return false;
}

bool PageSourceWithDelegates::CanClearUserInput(PageID pageID) const {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    return false;
  }

  auto wce = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
  if (wce) {
    return wce->CanClearUserInput(pageID);
  }
  return mDoodles->HaveDoodles(pageID);
}

void PageSourceWithDelegates::PostCursorEvent(
  KneeboardViewID ctx,
  const CursorEvent& event,
  PageID pageID) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    return;
  }

  auto wce = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
  if (wce) {
    wce->PostCursorEvent(ctx, event, pageID);
    return;
  }

  const auto contentSize = delegate->GetPreferredSize(pageID);
  if (!contentSize.has_value()) {
    return;
  }

  mDoodles->PostCursorEvent(ctx, event, pageID, contentSize->mPixelSize);
}

void PageSourceWithDelegates::ClearUserInput(PageID pageID) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    return;
  }

  const scope_exit updateState(
    [this]() { this->evAvailableFeaturesChangedEvent.Emit(); });

  auto withCursorEvents
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
  if (withCursorEvents) {
    withCursorEvents->ClearUserInput(pageID);
  } else {
    mDoodles->ClearPage(pageID);
  }
}

void PageSourceWithDelegates::ClearUserInput() {
  const scope_exit updateState(
    [this]() { this->evAvailableFeaturesChangedEvent.Emit(); });

  mDoodles->Clear();
  for (const auto& delegate: mDelegates) {
    auto withCursorEvents
      = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
    if (withCursorEvents) {
      withCursorEvents->ClearUserInput();
    }
  }
}

bool PageSourceWithDelegates::IsNavigationAvailable() const {
  return this->GetPageCount() > 2;
}

std::vector<NavigationEntry> PageSourceWithDelegates::GetNavigationEntries()
  const {
  std::vector<NavigationEntry> entries;
  for (const auto& delegate: mDelegates) {
    const auto withNavigation
      = std::dynamic_pointer_cast<IPageSourceWithNavigation>(delegate);
    if (!withNavigation) {
      continue;
    }
    const auto delegateEntries = withNavigation->GetNavigationEntries();
    std::ranges::copy(delegateEntries, std::back_inserter(entries));
  }
  return entries;
}

bool PageSourceWithDelegates::HasDeveloperTools(PageID id) const {
  auto delegate = this->FindDelegate(id);
  if (!delegate) {
    return false;
  }
  auto devTools
    = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(delegate);
  if (!devTools) {
    return false;
  }
  return devTools->HasDeveloperTools(id);
}

fire_and_forget PageSourceWithDelegates::OpenDeveloperToolsWindow(
  KneeboardViewID view,
  PageID page) {
  auto delegate = this->FindDelegate(page);
  if (!delegate) {
    co_return;
  }
  auto devTools
    = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(delegate);
  if (!devTools) {
    co_return;
  }
  devTools->OpenDeveloperToolsWindow(view, page);
  co_return;
}

}// namespace OpenKneeboard
