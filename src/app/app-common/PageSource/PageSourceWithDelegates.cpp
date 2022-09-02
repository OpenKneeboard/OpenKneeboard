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

#include <algorithm>
#include <numeric>

namespace OpenKneeboard {

PageSourceWithDelegates::PageSourceWithDelegates(
  const DXResources& dxr,
  KneeboardState* kbs) {
  mContentLayerCache = std::make_unique<CachedLayer>(dxr);
  mDoodleRenderer = std::make_unique<DoodleRenderer>(dxr, kbs);
}

PageSourceWithDelegates::~PageSourceWithDelegates() {
  for (auto& event: mDelegateEvents) {
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
    std::ranges::copy(
      std::vector<EventHandlerToken> {
        AddEventListener(
          delegate->evNeedsRepaintEvent, this->evNeedsRepaintEvent),
        AddEventListener(
          delegate->evPageAppendedEvent, this->evPageAppendedEvent),
        AddEventListener(
          delegate->evContentChangedEvent, this->evContentChangedEvent),
      },
      std::back_inserter(mDelegateEvents));
  }

  this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

uint16_t PageSourceWithDelegates::GetPageCount() const {
  return std::accumulate(
    mDelegates.begin(),
    mDelegates.end(),
    0,
    [](const auto& acc, const auto& delegate) {
      return acc + delegate->GetPageCount();
    });
};

D2D1_SIZE_U PageSourceWithDelegates::GetNativeContentSize(uint16_t pageIndex) {
  auto [delegate, decodedIndex] = DecodePageIndex(pageIndex);
  return delegate->GetNativeContentSize(decodedIndex);
}

void PageSourceWithDelegates::RenderPage(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  auto [delegate, decodedIndex] = DecodePageIndex(pageIndex);

  // If it has cursor events, let it do everything itself...
  auto withCursorEvents
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
  if (withCursorEvents) {
    delegate->RenderPage(ctx, decodedIndex, rect);
    return;
  }

  // ... otherwise, we'll assume it should be doodleable
  const auto nativeSize = delegate->GetNativeContentSize(decodedIndex);
  mContentLayerCache->Render(
    rect,
    nativeSize,
    pageIndex,
    ctx,
    [&](ID2D1DeviceContext* ctx, const D2D1_SIZE_U& size) {
      delegate->RenderPage(
        ctx,
        decodedIndex,
        {
          0.0f,
          0.0f,
          static_cast<FLOAT>(size.width),
          static_cast<FLOAT>(size.height),
        });
    });
  mDoodleRenderer->Render(ctx, pageIndex, rect);
}

void PageSourceWithDelegates::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& event,
  uint16_t pageIndex) {
  auto [delegate, decodedIndex] = DecodePageIndex(pageIndex);
  auto withCursorEvents
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(delegate);
  if (withCursorEvents) {
    withCursorEvents->PostCursorEvent(ctx, event, decodedIndex);
  } else {
    mDoodleRenderer->PostCursorEvent(
      ctx, event, pageIndex, delegate->GetNativeContentSize(decodedIndex));
  }
}

bool PageSourceWithDelegates::IsNavigationAvailable() const {
  return std::ranges::any_of(mDelegates, [](const auto& delegate) {
    auto withNavigation
      = std::dynamic_pointer_cast<IPageSourceWithNavigation>(delegate);
    return withNavigation && withNavigation->IsNavigationAvailable();
  });
}

std::vector<NavigationEntry> PageSourceWithDelegates::GetNavigationEntries()
  const {
  std::vector<NavigationEntry> entries;
  for (const auto& delegate: mDelegates) {
    auto withNavigation
      = std::dynamic_pointer_cast<IPageSourceWithNavigation>(delegate);
    if (withNavigation) {
      std::ranges::copy(
        withNavigation->GetNavigationEntries(), std::back_inserter(entries));
    }
  }
  return entries;
}

std::tuple<std::shared_ptr<IPageSource>, uint16_t>
PageSourceWithDelegates::DecodePageIndex(uint16_t pageIndex) const {
  uint16_t offset = 0;
  for (auto& delegate: mDelegates) {
    if (pageIndex - offset < delegate->GetPageCount()) {
      return {delegate, pageIndex - offset};
    }
    offset += delegate->GetPageCount();
  }
  return {nullptr, 0};
}

}// namespace OpenKneeboard
