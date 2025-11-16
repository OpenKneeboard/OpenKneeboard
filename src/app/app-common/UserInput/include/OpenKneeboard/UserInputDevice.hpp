// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/UserInputButtonBinding.hpp>

#include <string>
#include <unordered_set>

namespace OpenKneeboard {

class UserInputButtonEvent;
enum class UserAction;

class UserInputDevice : protected EventReceiver {
 public:
  UserInputDevice();
  ~UserInputDevice();

  virtual std::string GetName() const = 0;
  virtual std::string GetID() const = 0;

  virtual std::string GetButtonComboDescription(
    const std::unordered_set<uint64_t>& ids) const
    = 0;

  virtual std::vector<UserInputButtonBinding> GetButtonBindings() const = 0;
  virtual void SetButtonBindings(const std::vector<UserInputButtonBinding>&)
    = 0;

  Event<UserInputButtonEvent> evButtonEvent;
  /** Automatically emitted based on the bindings and evButtonEvent.
   *
   * Can be suppressed either by hooking this event directly, or by
   * hooking `evButtonEvent` (e.g. for the bindings UI)
   */
  Event<UserAction> evUserActionEvent;

 private:
  winrt::apartment_context mUIThread;
  void OnButtonEvent(UserInputButtonEvent);

  std::unordered_set<uint64_t> mActiveButtons;
};

}// namespace OpenKneeboard
