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
#include <OpenKneeboard/UserInputButtonEvent.h>
#include <OpenKneeboard/UserInputDevice.h>

namespace OpenKneeboard {

UserInputButtonEvent::UserInputButtonEvent(
  std::shared_ptr<UserInputDevice> device,
  uint64_t buttonID,
  bool pressed)
  : mDevice(device), mButtonID(buttonID), mPressed(pressed) {
}

UserInputButtonEvent::~UserInputButtonEvent() {
}

UserInputDevice* UserInputButtonEvent::GetDevice() const {
  return mDevice.get();
}

uint64_t UserInputButtonEvent::GetButtonID() const {
  return mButtonID;
}

bool UserInputButtonEvent::IsPressed() const {
  return mPressed;
}

}// namespace OpenKneeboard
