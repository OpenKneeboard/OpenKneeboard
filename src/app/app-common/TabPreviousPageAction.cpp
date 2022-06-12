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
#include <OpenKneeboard/TabPreviousPageAction.h>
#include <OpenKneeboard/TabState.h>

namespace OpenKneeboard {

TabPreviousPageAction::TabPreviousPageAction(TabState* state)
  : TabAction("\uE760", _("Previous Page")), mState(state) {
  AddEventListener(state->evNeedsRepaintEvent, this->evStateChangedEvent);
}

bool TabPreviousPageAction::IsEnabled() {
  return mState->GetPageIndex() > 0;
}

void TabPreviousPageAction::Execute() {
  mState->SetPageIndex(mState->GetPageIndex() - 1);
}

}// namespace OpenKneeboard
