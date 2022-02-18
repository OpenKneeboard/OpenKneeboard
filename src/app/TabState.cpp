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
#include "TabState.h"

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabWithCursorEvents.h>
#include <OpenKneeboard/TabWithNavigation.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

namespace OpenKneeboard {

TabState::TabState(const std::shared_ptr<Tab>& tab)
  : mRootTab(tab), mRootTabPage(0) {
  AddEventListener(tab->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    tab->evFullyReplacedEvent, &TabState::OnTabFullyReplaced, this);
  AddEventListener(
    tab->evPageAppendedEvent, &TabState::OnTabPageAppended, this);
}

TabState::~TabState() {
}

std::shared_ptr<Tab> TabState::GetRootTab() const {
  return mRootTab;
}

std::shared_ptr<Tab> TabState::GetTab() const {
  return mActiveSubTab ? mActiveSubTab : mRootTab;
}

uint16_t TabState::GetPageIndex() const {
  return mActiveSubTab ? mActiveSubTabPage : mRootTabPage;
}

void TabState::PostCursorEvent(const CursorEvent& ev) {
  auto receiver
    = std::dynamic_pointer_cast<TabWithCursorEvents>(this->GetTab());
  if (receiver) {
    receiver->PostCursorEvent(ev, this->GetPageIndex());
  }
}

uint16_t TabState::GetPageCount() const {
  return this->GetTab()->GetPageCount();
}

void TabState::SetPageIndex(uint16_t page) {
  if (page >= this->GetPageCount()) {
    return;
  }

  if (mActiveSubTab) {
    mActiveSubTabPage = page;
  } else {
    mRootTabPage = page;
  }

  evNeedsRepaintEvent.Emit();
  evPageChangedEvent.Emit();
}

void TabState::NextPage() {
  SetPageIndex(GetPageIndex() + 1);
}

void TabState::PreviousPage() {
  const auto page = GetPageIndex();
  if (page > 0) {
    SetPageIndex(page - 1);
  }
}

void TabState::OnTabFullyReplaced() {
  mRootTabPage = 0;
  if (!mActiveSubTab) {
    evNeedsRepaintEvent.Emit();
  }
  evPageChangedEvent.Emit();
}

void TabState::OnTabPageAppended() {
  const auto count = mRootTab->GetPageCount();
  if (mRootTabPage == count - 2) {
    if (mActiveSubTab) {
      mRootTabPage++;
    } else {
      NextPage();
    }
  }
}

D2D1_SIZE_U TabState::GetNativeContentSize() const {
  return GetTab()->GetNativeContentSize(GetPageIndex());
}

TabMode TabState::GetTabMode() const {
  return mTabMode;
}

bool TabState::SupportsTabMode(TabMode mode) const {
  switch (mode) {
    case TabMode::NORMAL:
      return true;
    case TabMode::NAVIGATION:
      return (bool)std::dynamic_pointer_cast<TabWithNavigation>(mRootTab);
  }
  // above switch should be exhaustive
  OPENKNEEBOARD_BREAK;
  return false;
}

bool TabState::SetTabMode(TabMode mode) {
  if (mTabMode == mode || !SupportsTabMode(mode)) {
    // Shouldn't have been called
    OPENKNEEBOARD_BREAK;
    return false;
  }

  auto receiver
    = std::dynamic_pointer_cast<TabWithCursorEvents>(this->GetTab());
  if (receiver) {
    receiver->PostCursorEvent({}, this->GetPageIndex());
  }

  mTabMode = mode;
  mActiveSubTab = nullptr;
  mActiveSubTabPage = 0;

  switch (mode) {
    case TabMode::NORMAL:
      break;
    case TabMode::NAVIGATION:
      mActiveSubTab = std::dynamic_pointer_cast<TabWithNavigation>(mRootTab)
                        ->CreateNavigationTab(mRootTabPage);
      AddEventListener(
        mActiveSubTab->evPageChangeRequestedEvent, [this](uint16_t newPage) {
          mRootTabPage = newPage;
          SetTabMode(TabMode::NORMAL);
        });
      break;
  }

  if (mode != TabMode::NORMAL && !mActiveSubTab) {
    // Switch should have been exhaustive
    OPENKNEEBOARD_BREAK;
  }

  evPageChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();

  return true;
}

}// namespace OpenKneeboard
