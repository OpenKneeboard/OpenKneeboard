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
  const DXResources& dxr,
  KneeboardState* kbs)
  : mDXResources(dxr) {
  mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);
  mFixedEvents = {
    AddEventListener(mDoodles->evNeedsRepaintEvent, this->evNeedsRepaintEvent),
    AddEventListener(
      mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent),
    AddEventListener(
      this->evContentChangedEvent,
      [this](ContentChangeType type) {
        this->mContentLayerCache.clear();
        if (type == ContentChangeType::FullyReplaced) {
          this->mDoodles->Clear();
        }
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

  this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
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
    std::ranges::copy(delegate->GetPageIDs(), std::back_inserter(ret));
  }
  return ret;
}

std::shared_ptr<IPageSource> PageSourceWithDelegates::FindDelegate(
  PageID pageID) const {
  auto delegate
    = std::ranges::find_if(mDelegates, [pageID](const auto& delegate) {
        auto pageIDs = delegate->GetPageIDs();
        auto it = std::ranges::find(pageIDs, pageID);
        return it != pageIDs.end();
      });
  if (delegate == mDelegates.end()) {
    return {nullptr};
  }
  return *delegate;
}

D2D1_SIZE_U PageSourceWithDelegates::GetNativeContentSize(PageID pageID) {
  auto delegate = this->FindDelegate(pageID);
  if (!delegate) {
    return {ErrorRenderWidth, ErrorRenderHeight};
  }
  return delegate->GetNativeContentSize(pageID);
}

void PageSourceWithDelegates::RenderPage(
  RenderTargetID rti,
  ID2D1DeviceContext* ctx,
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
    delegate->RenderPage(rti, ctx, pageID, rect);
    return;
  }

  // ... otherwise, we'll assume it should be doodleable

  if (!mContentLayerCache.contains(rti)) {
    mContentLayerCache[rti] = std::make_unique<CachedLayer>(mDXResources);
  }

  const auto nativeSize = delegate->GetNativeContentSize(pageID);
  mContentLayerCache[rti]->Render(
    rect,
    nativeSize,
    pageID.GetTemporaryValue(),
    ctx,
    [&](ID2D1DeviceContext* ctx, const D2D1_SIZE_U& size) {
      delegate->RenderPage(
        rti,
        ctx,
        pageID,
        {
          0.0f,
          0.0f,
          static_cast<FLOAT>(size.width),
          static_cast<FLOAT>(size.height),
        });
    });
  mDoodles->Render(ctx, pageID, rect);
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
  }

  mDoodles->PostCursorEvent(
    ctx, event, pageID, delegate->GetNativeContentSize(pageID));
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
  return this->GetPageCount() > 2 && !this->GetNavigationEntries().empty();
}

std::vector<NavigationEntry> PageSourceWithDelegates::GetNavigationEntries()
  const {
  std::vector<NavigationEntry> entries;
  PageIndex pageOffset = 0;
  for (const auto& delegate: mDelegates) {
    const scope_guard updateScopeOffset(
      [&]() { pageOffset += delegate->GetPageCount(); });

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
