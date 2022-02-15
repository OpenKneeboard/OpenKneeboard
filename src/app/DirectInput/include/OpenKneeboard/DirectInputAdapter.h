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

#include <shims/winrt.h>

#include <memory>
#include <nlohmann/json_fwd.hpp>

#include "Events.h"
#include "UserAction.h"

struct IDirectInput8W;

namespace OpenKneeboard {

struct DirectInputBinding;
struct DirectInputButtonEvent;

// Using DirectInput instead of wxJoystick so we can use
// all 128 buttons, rather than just the first 32
class DirectInputAdapter final : private OpenKneeboard::EventReceiver {
 public:
  DirectInputAdapter() = delete;
  DirectInputAdapter(const nlohmann::json& settings);
  virtual ~DirectInputAdapter();

  virtual nlohmann::json GetSettings() const;

  OpenKneeboard::Event<OpenKneeboard::UserAction> evUserAction;

  IDirectInput8W* GetDirectInput() const;
  std::vector<DirectInputBinding> GetBindings() const;
  void SetBindings(const std::vector<DirectInputBinding>&);

  void SetHook(std::function<void(const DirectInputButtonEvent&)>);

 private:
  winrt::com_ptr<IDirectInput8W> mDI8;
  std::vector<DirectInputBinding> mBindings;
  std::function<void(const DirectInputButtonEvent&)> mHook;

  std::jthread mDIThread;

  void OnDirectInputButtonEvent(const DirectInputButtonEvent&);
};

}// namespace OpenKneeboard
