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
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabNextPageAction.h>
#include <OpenKneeboard/TabView.h>

namespace OpenKneeboard {

TabNextPageAction::TabNextPageAction(
  KneeboardState* kneeboard,
  const std::shared_ptr<ITabView>& tabView)
  : TabAction("\uE761", _("Next Page")),
    mKneeboard(kneeboard),
    mTabView(tabView) {
  AddEventListener(tabView->evPageChangedEvent, this->evStateChangedEvent);
  AddEventListener(
    tabView->GetTab()->evPageAppendedEvent, this->evStateChangedEvent);
  AddEventListener(
    kneeboard->evSettingsChangedEvent, this->evStateChangedEvent);
}

TabNextPageAction::~TabNextPageAction() = default;

bool TabNextPageAction::IsEnabled() {
  const auto count = mTabView->GetPageCount();
  if (count < 2) {
    return false;
  }

  if (mKneeboard->GetAppSettings().mLoopPages) {
    return true;
  }

  return mTabView->GetPageIndex() + 1 < count;
}

void TabNextPageAction::Execute() {
  mTabView->NextPage();
}

}// namespace OpenKneeboard
