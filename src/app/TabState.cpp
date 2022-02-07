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

#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <shims/wx.h>
#include <wx/event.h>

#include "okEvents.h"

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
  this->GetTab()->PostCursorEvent(ev, this->GetPageIndex());
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

  evNeedsRepaintEvent();
  evPageChangedEvent();
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
    evNeedsRepaintEvent();
  }
  evPageChangedEvent();
}

void TabState::OnTabPageAppended() {
  const auto count = mRootTab->GetPageCount();
  if (mRootTabPage == count - 2) {
    NextPage();
  }
}

D2D1_SIZE_U TabState::GetNativeContentSize() const {
  return GetTab()->GetNativeContentSize(GetPageIndex());
}

}// namespace OpenKneeboard
