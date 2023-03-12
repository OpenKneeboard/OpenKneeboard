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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/IPageSourceWithNavigation.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/TabView.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

namespace OpenKneeboard {

TabView::TabView(
  const DXResources& dxr,
  KneeboardState* kneeboard,
  const std::shared_ptr<ITab>& tab)
  : mDXR(dxr), mKneeboard(kneeboard), mRootTab(tab) {
  const auto rootPageIDs = mRootTab->GetPageIDs();
  if (!rootPageIDs.empty()) {
    mRootTabPage = {rootPageIDs.front(), 0};
  }

  AddEventListener(tab->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    tab->evContentChangedEvent,
    std::bind_front(&TabView::OnTabContentChanged, this));
  AddEventListener(
    tab->evPageAppendedEvent,
    std::bind_front(&TabView::OnTabPageAppended, this));
  AddEventListener(
    this->evPageChangedEvent, this->evAvailableFeaturesChangedEvent);
  AddEventListener(
    tab->evPageChangeRequestedEvent, [this](EventContext ctx, PageID id) {
      if (ctx != mEventContext) {
        return;
      }
      this->SetPageID(id);
    });
  AddEventListener(
    tab->evAvailableFeaturesChangedEvent,
    this->evAvailableFeaturesChangedEvent);
  AddEventListener(tab->evBookmarksChangedEvent, this->evBookmarksChangedEvent);
}

TabView::~TabView() {
  this->RemoveAllEventListeners();
}

std::shared_ptr<ITab> TabView::GetRootTab() const {
  return mRootTab;
}

std::shared_ptr<ITab> TabView::GetTab() const {
  return mActiveSubTab ? mActiveSubTab : mRootTab;
}

PageID TabView::GetPageID() const {
  const auto mode = this->GetTabMode();
  if (mode != TabMode::NORMAL && mActiveSubTabPageID) {
    return *mActiveSubTabPageID;
  }
  if (mode == TabMode::NORMAL && mRootTabPage) {
    return mRootTabPage->mID;
  }

  const auto ids = this->GetTab()->GetPageIDs();
  if (ids.size() == 0) {
    return PageID(nullptr);
  }

  return ids.front();
}

std::vector<PageID> TabView::GetPageIDs() const {
  return mActiveSubTab ? mActiveSubTab->GetPageIDs() : mRootTab->GetPageIDs();
}

void TabView::PostCursorEvent(const CursorEvent& ev) {
  auto receiver
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(this->GetTab());
  if (!receiver) {
    return;
  }
  const auto size = this->GetNativeContentSize();
  CursorEvent tabEvent {ev};
  tabEvent.mX *= size.width;
  tabEvent.mY *= size.height;
  receiver->PostCursorEvent(mEventContext, tabEvent, this->GetPageID());
}

void TabView::SetPageID(PageID page) {
  const auto tab = this->GetTab();
  const auto pages = tab->GetPageIDs();
  const auto it = std::ranges::find(pages, page);
  if (it == pages.end()) {
    return;
  }

  if (mActiveSubTab) {
    mActiveSubTabPageID = page;
  } else {
    mRootTabPage = {page, static_cast<PageIndex>(it - pages.begin())};
  }

  this->PostCursorEvent({});

  evNeedsRepaintEvent.Emit();
  evPageChangedEvent.Emit();
}

void TabView::OnTabContentChanged() {
  const scope_guard updateOnExit([this] {
    this->evContentChangedEvent.Emit();
    if (!mActiveSubTab) {
      evNeedsRepaintEvent.Emit();
    }
  });

  const auto pages = mRootTab->GetPageIDs();
  if (pages.empty()) {
    mRootTabPage = {};
    evPageChangedEvent.Emit();
    return;
  }

  if (mRootTabPage) {
    const auto it = std::ranges::find(pages, mRootTabPage->mID);
    if (it == pages.end()) {
      // Fixed below
      mRootTabPage = {};
    }
  }

  if (!mRootTabPage) {
    mRootTabPage = {pages.front(), 0};
    evPageChangedEvent.Emit();
    return;
  }

  const auto it = std::ranges::find(pages, mRootTabPage->mID);
  if (it == pages.end()) {
    mRootTabPage = {pages.back(), static_cast<PageIndex>(pages.size() - 1)};
    evPageChangedEvent.Emit();
    return;
  }

  if (mRootTabPage->mIndex == 0 && it != pages.begin()) {
    mRootTabPage->mID = pages.front();
    evPageChangedEvent.Emit();
    return;
  }
}

void TabView::OnTabPageAppended(SuggestedPageAppendAction suggestedAction) {
  auto pages = mRootTab->GetPageIDs();
  if (pages.size() < 2 || !mRootTabPage) {
    mRootTabPage = {pages.front(), 0};
    evPageChangedEvent.Emit();
    evNeedsRepaintEvent.Emit();
    return;
  }

  if (suggestedAction == SuggestedPageAppendAction::KeepOnCurrentPage) {
    return;
  }

  if (mRootTabPage->mIndex != pages.size() - 2) {
    return;
  }

  mRootTabPage = {pages.back(), static_cast<PageIndex>(pages.size() - 1)};
  if (mActiveSubTab) {
    return;
  }

  evPageChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();
}

D2D1_SIZE_U TabView::GetNativeContentSize() const {
  return GetTab()->GetNativeContentSize(GetPageID());
}

TabMode TabView::GetTabMode() const {
  return mTabMode;
}

bool TabView::SupportsTabMode(TabMode mode) const {
  switch (mode) {
    case TabMode::NORMAL:
      return true;
    case TabMode::NAVIGATION: {
      auto nav = std::dynamic_pointer_cast<IPageSourceWithNavigation>(mRootTab);
      return nav && nav->IsNavigationAvailable();
    }
  }
  // above switch should be exhaustive
  OPENKNEEBOARD_BREAK;
  return false;
}

bool TabView::SetTabMode(TabMode mode) {
  if (mTabMode == mode || !SupportsTabMode(mode)) {
    return false;
  }

  auto receiver
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(this->GetTab());
  if (receiver) {
    receiver->PostCursorEvent(mEventContext, {}, this->GetPageID());
  }

  mTabMode = mode;
  mActiveSubTab = nullptr;
  mActiveSubTabPageID = {};

  switch (mode) {
    case TabMode::NORMAL:
      break;
    case TabMode::NAVIGATION:
      mActiveSubTab = std::make_shared<NavigationTab>(
        mDXR,
        mRootTab,
        std::dynamic_pointer_cast<IPageSourceWithNavigation>(mRootTab)
          ->GetNavigationEntries(),
        mRootTab->GetNativeContentSize(
          mRootTabPage ? (mRootTabPage->mID) : (PageID {nullptr})));
      AddEventListener(
        mActiveSubTab->evPageChangeRequestedEvent,
        [this](EventContext ctx, PageID newPage) {
          if (ctx != mEventContext) {
            return;
          }
          const auto ids = mRootTab->GetPageIDs();
          const auto it = std::ranges::find(ids, newPage);
          if (it == ids.end()) {
            return;
          }
          mRootTabPage = {newPage, static_cast<PageIndex>(it - ids.begin())};
          SetTabMode(TabMode::NORMAL);
        });
      AddEventListener(
        mActiveSubTab->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
      break;
  }

  if (mode != TabMode::NORMAL && !mActiveSubTab) {
    // Switch should have been exhaustive
    OPENKNEEBOARD_BREAK;
  }

  evPageChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();
  evTabModeChangedEvent.Emit();
  evContentChangedEvent.Emit();

  return true;
}

}// namespace OpenKneeboard
