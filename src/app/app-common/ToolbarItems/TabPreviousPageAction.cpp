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
#include <OpenKneeboard/AppSettings.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabPreviousPageAction.h>
#include <OpenKneeboard/TabView.h>

namespace OpenKneeboard {

TabPreviousPageAction::TabPreviousPageAction(
  KneeboardState* kneeboard,
  const std::shared_ptr<ITabView>& tabView)
  : ToolbarAction("\uE760", _("Previous Page")),
    mKneeboard(kneeboard),
    mTabView(tabView) {
  AddEventListener(tabView->evPageChangedEvent, this->evStateChangedEvent);
  AddEventListener(tabView->evContentChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    tabView->GetTab()->evPageAppendedEvent, this->evStateChangedEvent);
  AddEventListener(
    kneeboard->evSettingsChangedEvent, this->evStateChangedEvent);
}

TabPreviousPageAction::~TabPreviousPageAction() {
  this->RemoveAllEventListeners();
}

bool TabPreviousPageAction::IsEnabled() const {
  auto tv = mTabView.lock();
  if (!tv) {
    return false;
  }

  const auto pages = tv->GetPageIDs();
  if (pages.size() < 2) {
    return false;
  }

  if (mKneeboard->GetAppSettings().mLoopPages) {
    return true;
  }

  return tv->GetPageID() != pages.front();
}

void TabPreviousPageAction::Execute() {
  auto tv = mTabView.lock();
  if (!tv) {
    return;
  }

  const auto pages = tv->GetPageIDs();

  if (pages.size() < 2) {
    return;
  }

  auto it = std::ranges::find(pages, tv->GetPageID());
  if (it == pages.end()) {
    return;
  }

  if (it != pages.begin()) {
    it--;
  } else if (mKneeboard->GetAppSettings().mLoopPages) {
    it = pages.end() - 1;
  } else {
    return;
  }

  tv->SetPageID(*it);
}

}// namespace OpenKneeboard
