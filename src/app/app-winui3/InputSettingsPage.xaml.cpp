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
// clang-format off
#include "pch.h"
#include "InputSettingsPage.xaml.h"
#include "InputSettingsPage.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabletInputDevice.h>
#include <OpenKneeboard/UserInputDevice.h>

#include "Globals.h"

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

InputSettingsPage::InputSettingsPage() {
  InitializeComponent();
}

winrt::event_token InputSettingsPage::PropertyChanged(
  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
    handler) {
  return mPropertyChangedEvent.add(handler);
}

void InputSettingsPage::PropertyChanged(
  winrt::event_token const& token) noexcept {
  mPropertyChangedEvent.remove(token);
}

IVector<IInspectable> InputSettingsPage::Devices() {
  auto devices {winrt::single_threaded_vector<IInspectable>()};
  for (const auto& device: gKneeboard->GetInputDevices()) {
    OpenKneeboardApp::InputDeviceUIData deviceData {nullptr};
    auto tablet = std::dynamic_pointer_cast<TabletInputDevice>(device);
    if (tablet) {
      auto tabletData = OpenKneeboardApp::TabletInputDeviceUIData {};
      tabletData.Orientation(static_cast<uint8_t>(tablet->GetOrientation()));
      deviceData = tabletData;
    } else {
      deviceData = OpenKneeboardApp::InputDeviceUIData {};
    }
    deviceData.Name(to_hstring(device->GetName()));
    deviceData.DeviceID(to_hstring(device->GetID()));
    devices.Append(deviceData);
  }
  return devices;
}

void InputSettingsPage::OnOrientationChanged(
  const IInspectable& sender,
  const SelectionChangedEventArgs&) {
  auto combo = sender.as<ComboBox>();
  auto deviceID = to_string(unbox_value<hstring>(combo.Tag()));

  auto devices = gKneeboard->GetInputDevices();
  auto it = std::ranges::find_if(
    devices, [&](auto it) { return it->GetID() == deviceID; });
  if (it == devices.end()) {
    return;
  }
  auto& device = *it;

  std::dynamic_pointer_cast<TabletInputDevice>(device)->SetOrientation(
    static_cast<TabletOrientation>(combo.SelectedIndex()));
}

bool InputSettingsPage::FixedWriteRadius() {
  return gKneeboard->GetDoodleSettings().FixedDraw;
}
void InputSettingsPage::FixedWriteRadius(bool value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.FixedDraw = value;
  gKneeboard->SetDoodleSettings(ds);
}
bool InputSettingsPage::FixedEraseRadius() {
  return gKneeboard->GetDoodleSettings().FixedErase;
}
void InputSettingsPage::FixedEraseRadius(bool value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.FixedErase = value;
  gKneeboard->SetDoodleSettings(ds);
}
unsigned int InputSettingsPage::EraseRadius() {
  return gKneeboard->GetDoodleSettings().EraseRadius;
}
void InputSettingsPage::EraseRadius(unsigned int value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.EraseRadius = value;
  gKneeboard->SetDoodleSettings(ds);
}
unsigned int InputSettingsPage::WriteRadius() {
  return gKneeboard->GetDoodleSettings().DrawRadius;
}
void InputSettingsPage::WriteRadius(unsigned int value) {
  auto ds = gKneeboard->GetDoodleSettings();
  ds.DrawRadius = value;
  gKneeboard->SetDoodleSettings(ds);
}

}// namespace winrt::OpenKneeboardApp::implementation
