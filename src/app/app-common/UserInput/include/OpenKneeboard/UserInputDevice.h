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

#include <OpenKneeboard/Events.h>

#include <string>
#include <unordered_set>

namespace OpenKneeboard {

class UserInputButtonBinding;
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
