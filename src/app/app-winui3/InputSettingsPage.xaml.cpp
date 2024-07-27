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

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabletInputDevice.h>
#include <OpenKneeboard/UserInputDevice.h>

#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/weak_wrap.h>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

InputSettingsPage::InputSettingsPage() {
  InitializeComponent();
  mKneeboard = gKneeboard.lock();

  AddEventListener(
    mKneeboard->evInputDevicesChangedEvent,
    weak_wrap(this)([](auto self) -> winrt::fire_and_forget {
      co_await self->mUIThread;
      self->mPropertyChangedEvent(*self, PropertyChangedEventArgs(L"Devices"));
    }));
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

  mKneeboard->ResetDirectInputSettings();
  mKneeboard->ResetTabletInputSettings();

  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"Devices"));
}

IVector<IInspectable> InputSettingsPage::Devices() noexcept {
  auto devices {winrt::single_threaded_vector<IInspectable>()};
  for (const auto& device: mKneeboard->GetInputDevices()) {
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

  auto devices = mKneeboard->GetInputDevices();
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
    mKneeboard->GetTabletInputAdapter()->GetWintabMode());
}

winrt::Windows::Foundation::IAsyncAction InputSettingsPage::WintabMode(
  uint8_t rawMode) const {
  auto t = mKneeboard->GetTabletInputAdapter();
  const auto mode = static_cast<OpenKneeboard::WintabMode>(rawMode);
  if (mode == t->GetWintabMode()) {
    co_return;
  }
  co_await t->SetWintabMode(mode);
}

bool InputSettingsPage::IsOpenTabletDriverEnabled() const {
  return mKneeboard->GetTabletInputAdapter()->IsOTDIPCEnabled();
}

bool InputSettingsPage::IsWinTabAvailable() const {
  return mKneeboard->GetTabletInputAdapter()->GetWinTabAvailability()
    == WinTabAvailability::Available;
}

winrt::hstring InputSettingsPage::WinTabAvailability() const {
  switch (mKneeboard->GetTabletInputAdapter()->GetWinTabAvailability()) {
    case WinTabAvailability::NotInstalled:
      return _(
        L"No 64-bit WinTab-capable tablet driver is installed on your "
        L"system.");
    case WinTabAvailability::Available:
      return _(L"WinTab is available on your system.");
    case WinTabAvailability::Skipping_OpenTabletDriverEnabled:
      return _(
        L"WinTab is disabled because you have enabled OpenTabletDriver.");
    case WinTabAvailability::Skipping_NoTrustedSignature:
      return _(
        L"WinTab is disabled because your manufacturer's WinTab driver is "
        L"not signed by a manufacturer that Windows recognizes and trusts; "
        L"historically, these drivers frequently cause OpenKneeboard and "
        L"game crashes. If there is not a more recent driver available, "
        L"use OpenTabletDriver instead.");
  }
  OPENKNEEBOARD_UNREACHABLE;
}

void InputSettingsPage::IsOpenTabletDriverEnabled(bool value) {
  mKneeboard->GetTabletInputAdapter()->SetIsOTDIPCEnabled(value);

  this->mPropertyChangedEvent(
    *this,
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(
      L"IsWinTabAvailable"));
  this->mPropertyChangedEvent(
    *this,
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(
      L"WinTabAvailability"));
}

}// namespace winrt::OpenKneeboardApp::implementation
