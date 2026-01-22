// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "UserInputDevice.hpp"

#include <dinput.h>

#include <memory>

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
