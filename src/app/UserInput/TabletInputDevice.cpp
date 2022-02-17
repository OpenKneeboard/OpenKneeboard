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

#include <OpenKneeboard/TabletInputDevice.h>
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <fmt/format.h>
#include <shims/wx.h>

namespace OpenKneeboard {

TabletInputDevice::TabletInputDevice(
  const std::string& name,
  const std::string& id)
  : mName(name), mID(id) {
}

TabletInputDevice::~TabletInputDevice() {
}

std::string TabletInputDevice::GetName() const {
  return mName;
}

std::string TabletInputDevice::GetID() const {
  return mID;
}

std::string TabletInputDevice::GetButtonComboDescription(
  const std::unordered_set<uint64_t>& ids) const {
  if (ids.empty()) {
    return _("None").utf8_string();
  }
  if (ids.size() == 1) {
    return fmt::format(
      fmt::runtime(_("Key {}").utf8_string()), *ids.begin() + 1);
  }
  std::string out;
  for (auto id: ids) {
    if (!out.empty()) {
      out += " + ";
    }
    out += fmt::to_string(id + 1);
  }
  return out;
}

std::vector<UserInputButtonBinding> TabletInputDevice::GetButtonBindings()
  const {
  return mButtonBindings;
}

void TabletInputDevice::SetButtonBindings(
  const std::vector<UserInputButtonBinding>& bindings) {
  mButtonBindings = bindings;
  evBindingsChangedEvent();
}

}// namespace OpenKneeboard
