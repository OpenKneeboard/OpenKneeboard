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
#include <OpenKneeboard/TabNavigationAction.h>
#include <OpenKneeboard/TabState.h>

namespace OpenKneeboard {

TabNavigationAction::TabNavigationAction(TabState* state)
  : TabToggleAction("\uE700", _("Contents")), mState(state) {
  AddEventListener(
    state->evAvailableFeaturesChangedEvent, this->evStateChangedEvent);
  AddEventListener(state->evTabModeChangedEvent, this->evStateChangedEvent);
}

bool TabNavigationAction::IsEnabled() {
  return mState->SupportsTabMode(TabMode::NAVIGATION);
}

bool TabNavigationAction::IsActive() {
  return mState->GetTabMode() == TabMode::NAVIGATION;
}

void TabNavigationAction::Activate() {
  mState->SetTabMode(TabMode::NAVIGATION);
}

void TabNavigationAction::Deactivate() {
  mState->SetTabMode(TabMode::NORMAL);
}

}// namespace OpenKneeboard
