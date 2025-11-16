// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/UserInputButtonEvent.hpp>
#include <OpenKneeboard/UserInputDevice.hpp>

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
