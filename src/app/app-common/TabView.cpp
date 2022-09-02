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

namespace OpenKneeboard {

TabView::TabView(
  const DXResources& dxr,
  KneeboardState* kneeboard,
  const std::shared_ptr<ITab>& tab)
  : mDXR(dxr), mKneeboard(kneeboard), mRootTab(tab), mRootTabPage(0) {
  AddEventListener(tab->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    tab->evContentChangedEvent, &TabView::OnTabContentChanged, this);
  AddEventListener(tab->evPageAppendedEvent, &TabView::OnTabPageAppended, this);
  AddEventListener(
    tab->evPageChangeRequestedEvent, [this](EventContext ctx, uint16_t index) {
      if (ctx != mEventContext) {
        return;
      }
      this->SetPageIndex(index);
    });
  AddEventListener(
    tab->evAvailableFeaturesChangedEvent,
    this->evAvailableFeaturesChangedEvent);
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

uint16_t TabView::GetPageIndex() const {
  return mActiveSubTab ? mActiveSubTabPage : mRootTabPage;
}

void TabView::PostCursorEvent(const CursorEvent& ev) {
  auto receiver
    = std::dynamic_pointer_cast<IPageSourceWithCursorEvents>(this->GetTab());
  if (receiver) {
    receiver->PostCursorEvent(mEventContext, ev, this->GetPageIndex());
  }
}

uint16_t TabView::GetPageCount() const {
  return this->GetTab()->GetPageCount();
}

void TabView::SetPageIndex(uint16_t page) {
  if (page >= this->GetPageCount()) {
    return;
  }

  if (mActiveSubTab) {
    mActiveSubTabPage = page;
  } else {
    mRootTabPage = page;
  }

  this->PostCursorEvent({});

  evNeedsRepaintEvent.Emit();
  evPageChangedEvent.Emit();
}

void TabView::NextPage() {
  const auto count = this->GetPageCount();
  if (count < 2) {
    return;
  }

  const auto current = this->GetPageIndex();
  if (current + 1 < count) {
    this->SetPageIndex(current + 1);
    return;
  }

  if (mKneeboard->GetAppSettings().mLoopPages) {
    this->SetPageIndex(0);
  }
}

void TabView::PreviousPage() {
  const auto page = GetPageIndex();
  if (page > 0) {
    this->SetPageIndex(page - 1);
    return;
  }

  if (!mKneeboard->GetAppSettings().mLoopPages) {
    return;
  }

  const auto count = this->GetPageCount();
  if (count > 2) {
    this->SetPageIndex(count - 1);
  }
}

void TabView::OnTabContentChanged(ContentChangeType changeType) {
  this->evContentChangedEvent.Emit(changeType);
  if (changeType == ContentChangeType::FullyReplaced) {
    mRootTabPage = 0;
    evPageChangedEvent.Emit();
  } else {
    const auto pageCount = mRootTab->GetPageCount();
    if (mRootTabPage >= pageCount) {
      mRootTabPage = std::max<uint16_t>(pageCount - 1, 0);
      evPageChangedEvent.Emit();
    }
  }
  evAvailableFeaturesChangedEvent.Emit();
  if (!mActiveSubTab) {
    evNeedsRepaintEvent.Emit();
  }
}

void TabView::OnTabPageAppended() {
  const auto count = mRootTab->GetPageCount();
  if (mRootTabPage == count - 2) {
    if (mActiveSubTab) {
      mRootTabPage++;
    } else {
      NextPage();
    }
  }
}

D2D1_SIZE_U TabView::GetNativeContentSize() const {
  return GetTab()->GetNativeContentSize(GetPageIndex());
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
    receiver->PostCursorEvent(mEventContext, {}, this->GetPageIndex());
  }

  mTabMode = mode;
  mActiveSubTab = nullptr;
  mActiveSubTabPage = 0;

  switch (mode) {
    case TabMode::NORMAL:
      break;
    case TabMode::NAVIGATION:
      mActiveSubTab = std::make_shared<NavigationTab>(
        mDXR,
        mRootTab,
        std::dynamic_pointer_cast<IPageSourceWithNavigation>(mRootTab)
          ->GetNavigationEntries(),
        mRootTab->GetNativeContentSize(mRootTabPage));
      AddEventListener(
        mActiveSubTab->evPageChangeRequestedEvent,
        [this](EventContext ctx, uint16_t newPage) {
          if (ctx != mEventContext) {
            return;
          }
          mRootTabPage = newPage;
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
  evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
  evAvailableFeaturesChangedEvent.Emit();

  return true;
}

}// namespace OpenKneeboard
