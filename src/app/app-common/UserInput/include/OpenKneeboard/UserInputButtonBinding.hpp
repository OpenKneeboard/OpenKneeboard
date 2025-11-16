// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/UserAction.hpp>

#include <memory>
#include <unordered_set>

namespace OpenKneeboard {

class UserInputDevice;

class UserInputButtonBinding final {
 public:
  UserInputButtonBinding(
    std::shared_ptr<UserInputDevice> device,
    std::unordered_set<uint64_t> buttons,
    UserAction action);
  ~UserInputButtonBinding();

  UserInputDevice* GetDevice() const;
  std::unordered_set<uint64_t> GetButtonIDs() const;
  UserAction GetAction() const;

 private:
  std::shared_ptr<UserInputDevice> mDevice;
  std::unordered_set<uint64_t> mButtons;
  UserAction mAction;
};

}// namespace OpenKneeboard
