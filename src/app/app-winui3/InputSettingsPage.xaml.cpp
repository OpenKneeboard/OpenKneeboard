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
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabletInputDevice.h>
#include <OpenKneeboard/UserInputDevice.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/weak_wrap.h>

#include "Globals.h"

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

InputSettingsPage::InputSettingsPage() {
  InitializeComponent();
  AddEventListener(
    gKneeboard->evInputDevicesChangedEvent,
    weak_wrap(
      [](auto self) -> winrt::fire_and_forget {
        co_await self->mUIThread;
        self->mPropertyChangedEvent(
          *self, PropertyChangedEventArgs(L"Devices"));
      },
      this));
}

InputSettingsPage::~InputSettingsPage() {
  this->RemoveAllEventListeners();
}

fire_and_forget InputSettingsPage::RestoreDefaults(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default input settings, "
                           "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  gKneeboard->ResetDirectInputSettings();
  gKneeboard->ResetTabletInputSettings();

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"Devices"));
}

IVector<IInspectable> InputSettingsPage::Devices() noexcept {
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

uint8_t InputSettingsPage::WintabMode() const {
  return static_cast<uint8_t>(
    gKneeboard->GetTabletInputAdapter()->GetWintabMode());
}

winrt::Windows::Foundation::IAsyncAction InputSettingsPage::WintabMode(
  uint8_t rawMode) const {
  auto t = gKneeboard->GetTabletInputAdapter();
  const auto mode = static_cast<OpenKneeboard::WintabMode>(rawMode);
  if (mode == t->GetWintabMode()) {
    co_return;
  }
  co_await t->SetWintabMode(mode);
}

bool InputSettingsPage::IsOpenTabletDriverEnabled() const {
  return gKneeboard->GetTabletInputAdapter()->IsOTDIPCEnabled();
}

void InputSettingsPage::IsOpenTabletDriverEnabled(bool value) {
  gKneeboard->GetTabletInputAdapter()->SetIsOTDIPCEnabled(value);
}

}// namespace winrt::OpenKneeboardApp::implementation
