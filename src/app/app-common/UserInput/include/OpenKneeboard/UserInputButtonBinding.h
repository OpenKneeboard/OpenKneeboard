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
#pragma once

#include <memory>
#include <unordered_set>

#include <OpenKneeboard/UserAction.h>

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
