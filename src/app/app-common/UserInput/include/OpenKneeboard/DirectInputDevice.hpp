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

#include "UserInputDevice.hpp"

#include <memory>

#include <dinput.h>

namespace OpenKneeboard {

class DirectInputDevice final
  : public UserInputDevice,
    public std::enable_shared_from_this<DirectInputDevice> {
 public:
  DirectInputDevice() = delete;
  static std::shared_ptr<DirectInputDevice> Create(
    const DIDEVICEINSTANCEW& instance);

  virtual std::string GetName() const override;
  virtual std::string GetID() const override;
  virtual std::string GetButtonComboDescription(
    const std::unordered_set<uint64_t>& ids) const override;

  virtual std::vector<UserInputButtonBinding> GetButtonBindings()
    const override;
  virtual void SetButtonBindings(
    const std::vector<UserInputButtonBinding>&) override;

  Event<> evBindingsChangedEvent;

  DIDEVICEINSTANCEW GetDIDeviceInstance() const;

  void PostButtonStateChange(uint8_t index, bool pressed);
  void PostHatStateChange(uint8_t hatIndex, DWORD oldValue, DWORD newValue);

  enum class VScrollDirection {
    Up,
    Down,
  };

  void PostVScroll(VScrollDirection);

 private:
  DirectInputDevice(const DIDEVICEINSTANCEW&);

  DIDEVICEINSTANCEW mDevice;
  std::vector<UserInputButtonBinding> mButtonBindings;

  std::string GetButtonLabel(uint64_t button) const;

  std::string GetKeyLabel(uint64_t key) const;
};

}// namespace OpenKneeboard
