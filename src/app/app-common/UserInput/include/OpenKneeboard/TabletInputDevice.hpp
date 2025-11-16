// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "UserInputDevice.hpp"

#include <OpenKneeboard/TabletSettings.hpp>

#include <shims/nlohmann/json.hpp>

namespace OpenKneeboard {

class TabletInputDevice final : public UserInputDevice {
 public:
  TabletInputDevice(
    const std::string& name,
    const std::string& id,
    TabletOrientation);
  ~TabletInputDevice();

  virtual std::string GetName() const override;
  virtual std::string GetID() const override;
  virtual std::string GetButtonComboDescription(
    const std::unordered_set<uint64_t>& ids) const override;

  virtual std::vector<UserInputButtonBinding> GetButtonBindings()
    const override;
  virtual void SetButtonBindings(
    const std::vector<UserInputButtonBinding>&) override;

  Event<> evBindingsChangedEvent;

  TabletOrientation GetOrientation() const;
  void SetOrientation(TabletOrientation);
  Event<TabletOrientation> evOrientationChangedEvent;

 private:
  std::string mName;
  std::string mID;
  std::vector<UserInputButtonBinding> mButtonBindings;
  TabletOrientation mOrientation;
};

}// namespace OpenKneeboard
