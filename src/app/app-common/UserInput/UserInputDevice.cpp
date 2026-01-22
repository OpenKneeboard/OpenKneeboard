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

UserInputDevice::UserInputDevice() {
  AddEventListener(
    evButtonEvent, std::bind_front(&UserInputDevice::OnButtonEvent, this));
}

UserInputDevice::~UserInputDevice() { this->RemoveAllEventListeners(); }

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
    bool foundReleasedButton = false;
    for (auto button: boundButtons) {
      if (button == ev.GetButtonID()) {
        foundReleasedButton = true;
      }
      if (!buttons.contains(button)) {
        goto NEXT_BINDING;
      }
    }
    if (!foundReleasedButton) {
      continue;
    }
    evUserActionEvent.EnqueueForContext(mUIThread, binding.GetAction());
    return;
  NEXT_BINDING:
    continue;// need a statement after label
  }
}

}// namespace OpenKneeboard
