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

#include <OpenKneeboard/TabletSettings.h>

#include <nlohmann/json.hpp>

#include "UserInputDevice.h"

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
