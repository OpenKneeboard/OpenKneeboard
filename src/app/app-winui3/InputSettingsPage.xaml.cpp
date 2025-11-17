// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "InputSettingsPage.xaml.h"
#include "InputSettingsPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TabletInputAdapter.hpp>
#include <OpenKneeboard/TabletInputDevice.hpp>
#include <OpenKneeboard/UserInputDevice.hpp>

#include <OpenKneeboard/utf8.hpp>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

InputSettingsPage::InputSettingsPage() {
  InitializeComponent();
  mKneeboard = gKneeboard.lock();

  AddEventListener(
    mKneeboard->evInputDevicesChangedEvent,
    [](auto self) { self->EmitPropertyChangedEvent(L"Devices"); }
      | bind_refs_front(this) | bind_winrt_context(mUIThread));
}

InputSettingsPage::~InputSettingsPage() {
  this->RemoveAllEventListeners();
}

OpenKneeboard::fire_and_forget InputSettingsPage::RestoreDefaults(
  IInspectable,
  RoutedEventArgs) noexcept {
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

  co_await mKneeboard->ResetDirectInputSettings();
  co_await mKneeboard->ResetTabletInputSettings();

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

}// namespace winrt::OpenKneeboardApp::implementation
