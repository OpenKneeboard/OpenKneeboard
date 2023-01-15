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
#include <OpenKneeboard/TabFirstPageAction.h>
#include <OpenKneeboard/TabView.h>

namespace OpenKneeboard {

TabFirstPageAction::TabFirstPageAction(const std::shared_ptr<ITabView>& state)
  : ToolbarAction("\uE892", _("First Page")), mTabView(state) {
  AddEventListener(state->evPageChangedEvent, this->evStateChangedEvent);
  AddEventListener(state->evContentChangedEvent, this->evStateChangedEvent);
}

TabFirstPageAction::~TabFirstPageAction() {
  this->RemoveAllEventListeners();
}

bool TabFirstPageAction::IsEnabled() const {
  auto tv = mTabView.lock();
  return tv && tv->GetPageCount() > 1 && tv->GetPageIndex() > 0;
}

void TabFirstPageAction::Execute() {
  if (auto tv = mTabView.lock()) {
    tv->SetPageIndex(0);
  }
}

}// namespace OpenKneeboard
