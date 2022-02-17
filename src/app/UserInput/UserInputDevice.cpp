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
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <OpenKneeboard/UserInputButtonEvent.h>
#include <OpenKneeboard/UserInputDevice.h>

namespace OpenKneeboard {

UserInputDevice::UserInputDevice() {
  AddEventListener(evButtonEvent, &UserInputDevice::OnButtonEvent, this);
}

UserInputDevice::~UserInputDevice() {
}

void UserInputDevice::OnButtonEvent(UserInputButtonEvent ev) {
  if (ev.IsPressed()) {
    mActiveButtons.insert(ev.GetButtonID());
    return;
  }

  // We act on release, but need to check the previous button
  // set. For example, if binding is shift+L and L is released,
  // the new active button state is just shift, but we need to
  // check for shift+L
  const auto buttons = mActiveButtons;
  mActiveButtons.erase(ev.GetButtonID());

  for (const auto& binding: this->GetButtonBindings()) {
    const auto boundButtons = binding.GetButtonIDs();
    if (buttons.size() < boundButtons.size()) {
      continue;
    }
    for (auto button: buttons) {
      if (!boundButtons.contains(button)) {
        goto NEXT_BINDING;
      }
    }
    evUserActionEvent(binding.GetAction());
    return;
  NEXT_BINDING:
    continue;// need a statement after label
  }
}

}// namespace OpenKneeboard
