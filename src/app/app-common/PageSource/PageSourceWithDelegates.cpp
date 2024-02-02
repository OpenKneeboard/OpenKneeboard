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
#include <OpenKneeboard/CachedLayer.h>
#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>

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
  for (auto& event: mDelegateEvents) {
    this->RemoveEventListener(event);
  }
  for (auto& event: mFixedEvents) {
    this->RemoveEventListener(event);
  }
}

void PageSourceWithDelegates::SetDelegates(
  const std::vector<std::shared_ptr<IPageSource>>& delegates) {
  mPageDelegates.clear();

  for (auto& event: mDelegateEvents) {
    this->RemoveEventListener(event);
  }
  mDelegateEvents.clear();

  mDelegates = delegates;

  for (auto& delegate: delegates) {
    std::weak_ptr<IPageSource> weakDelegate {delegate};
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

PreferredSize PageSourceWithDelegates::GetPreferredSize(PageID pageID) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    return {ErrorRenderSize, ScalingKind::Vector};
  }
  return delegate->GetPreferredSize(pageID);
}

void PageSourceWithDelegates::RenderPage(
  RenderTarget* rt,
  PageID pageID,
  const D2D1_RECT_F& rect) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  // If it has cursor events, let it do everything itself...
  auto withCursorEvents
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
  if (withCursorEvents) {
    delegate->RenderPage(rt, pageID, rect);
    return;
  }

  // ... otherwise, we'll assume it should be doodleable

  const auto rtid = rt->GetID();
  if (!mContentLayerCache.contains(rtid)) {
    mContentLayerCache[rtid] = std::make_unique<CachedLayer>(mDXResources);
  }

  mContentLayerCache[rtid]->Render(
    rect,
    pageID.GetTemporaryValue(),
    rt,
    [&](RenderTarget* rt, const D2D1_SIZE_U& size) {
      delegate->RenderPage(
        rt,
        pageID,
        {
          0.0f,
          0.0f,
          static_cast<FLOAT>(size.width),
          static_cast<FLOAT>(size.height),
        });
    });
  mDoodles->Render(rt->d2d(), pageID, rect);
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
  EventContext ctx,
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

  mDoodles->PostCursorEvent(
    ctx, event, pageID, delegate->GetPreferredSize(pageID).mPixelSize);
}

void PageSourceWithDelegates::ClearUserInput(PageID pageID) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    return;
  }

  const scope_guard updateState(
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
  const scope_guard updateState(
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

}// namespace OpenKneeboard
