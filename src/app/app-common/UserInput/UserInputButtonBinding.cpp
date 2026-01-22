// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/UserInputButtonBinding.hpp>
#include <OpenKneeboard/UserInputButtonEvent.hpp>
#include <OpenKneeboard/UserInputDevice.hpp>

namespace OpenKneeboard {

UserInputButtonBinding::UserInputButtonBinding(
  std::shared_ptr<UserInputDevice> device,
  std::unordered_set<uint64_t> buttons,
  UserAction action)
  : mDevice(device),
    mButtons(buttons),
    mAction(action) {}

UserInputButtonBinding::~UserInputButtonBinding() {}

UserInputDevice* UserInputButtonBinding::GetDevice() const {
  return mDevice.get();
}

std::unordered_set<uint64_t> UserInputButtonBinding::GetButtonIDs() const {
  return mButtons;
}

UserAction UserInputButtonBinding::GetAction() const { return mAction; }

}// namespace OpenKneeboard
