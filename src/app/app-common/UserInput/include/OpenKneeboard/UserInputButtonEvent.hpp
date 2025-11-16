// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <memory>
#include <string>

namespace OpenKneeboard {

class UserInputDevice;

class UserInputButtonEvent final {
 public:
  UserInputButtonEvent(
    std::shared_ptr<UserInputDevice> device,
    uint64_t buttonID,
    bool pressed);
  ~UserInputButtonEvent();

  UserInputDevice* GetDevice() const;
  uint64_t GetButtonID() const;
  bool IsPressed() const;

 private:
  std::shared_ptr<UserInputDevice> mDevice;
  int64_t mButtonID;
  bool mPressed;
};

}// namespace OpenKneeboard
