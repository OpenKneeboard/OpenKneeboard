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

#include <shims/wx.h>

#include <memory>
#include <vector>
#include <unordered_set>

#include <OpenKneeboard/Events.h>

namespace OpenKneeboard {
enum class UserAction;
class UserInputButtonEvent;
class UserInputDevice;
}// namespace OpenKneeboard

class okUserInputSettings final : public wxPanel {
 public:
  okUserInputSettings(
    wxWindow* parent,
    const std::vector<std::shared_ptr<OpenKneeboard::UserInputDevice>>&
      devices);
  ~okUserInputSettings();

  OpenKneeboard::Event<> evSettingsChangedEvent;

 private:
  struct DeviceButtons;
  std::vector<std::shared_ptr<OpenKneeboard::UserInputDevice>> mDevices;
  std::vector<DeviceButtons> mDeviceButtons;

  wxButton* CreateBindButton(
    wxWindow* parent,
    int deviceIndex,
    OpenKneeboard::UserAction action);

  wxDialog* CreateBindInputDialog(bool haveExistingBinding);

  void OnBindButton(
    wxCommandEvent& ev,
    int deviceIndex,
    OpenKneeboard::UserAction action);
  OpenKneeboard::EventBase::HookResult OnBindButtonEvent(
    wxDialog*,
    int deviceIndex,
    OpenKneeboard::UserAction action,
    std::unordered_set<uint64_t>* buttonState,
    const OpenKneeboard::UserInputButtonEvent&
  );
};
