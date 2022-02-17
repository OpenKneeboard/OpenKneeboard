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

#include <OpenKneeboard/DirectInputDevice.h>
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <OpenKneeboard/utf8.h>
#include <fmt/format.h>
#include <shims/winrt.h>
#include <shims/wx.h>

namespace OpenKneeboard {

DirectInputDevice::DirectInputDevice(const DIDEVICEINSTANCEW& device)
  : mDevice(device) {
}

std::string DirectInputDevice::GetName() const {
  return to_utf8(mDevice.tszInstanceName);
}

std::string DirectInputDevice::GetID() const {
  return to_utf8(winrt::to_hstring(mDevice.guidInstance));
}

std::string DirectInputDevice::GetButtonComboDescription(const std::unordered_set<uint64_t>& ids) const {
  if (ids.empty()) {
    return _("None").utf8_string();
  }
  if (ids.size() == 1) {
    return fmt::format(fmt::runtime(_("Button {}").utf8_string()), *ids.begin() + 1);
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

std::vector<UserInputButtonBinding> DirectInputDevice::GetButtonBindings()
  const {
  return mButtonBindings;
}

void DirectInputDevice::SetButtonBindings(
  const std::vector<UserInputButtonBinding>& bindings) {
  mButtonBindings = bindings;
  evBindingsChangedEvent();
}

DIDEVICEINSTANCEW DirectInputDevice::GetDIDeviceInstance() const {
  return mDevice;
}

}// namespace OpenKneeboard
