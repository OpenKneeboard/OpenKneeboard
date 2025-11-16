// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"

#include "InputDeviceUIData.h"
#include "InputDeviceUIData.g.cpp"
#include "TabletInputDeviceUIData.g.cpp"
#include "InputDeviceUIDataTemplateSelector.g.cpp"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {
hstring InputDeviceUIData::Name() {
  return mName;
}
void InputDeviceUIData::Name(const hstring& value) {
  mName = value;
}
hstring InputDeviceUIData::DeviceID() {
  return mDeviceID;
}
void InputDeviceUIData::DeviceID(const hstring& value) {
  mDeviceID = value;
}

uint8_t TabletInputDeviceUIData::Orientation() {
  return mOrientation;
}

void TabletInputDeviceUIData::Orientation(uint8_t value) {
  mOrientation = value;
}

DataTemplate InputDeviceUIDataTemplateSelector::GenericDevice() {
  return mGenericDevice;
}

void InputDeviceUIDataTemplateSelector::GenericDevice(
  const DataTemplate& value) {
  mGenericDevice = value;
}

DataTemplate InputDeviceUIDataTemplateSelector::TabletDevice() {
  return mTabletDevice;
}

void InputDeviceUIDataTemplateSelector::TabletDevice(
  const DataTemplate& value) {
  mTabletDevice = value;
}

DataTemplate InputDeviceUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item) {
  if (item.try_as<winrt::OpenKneeboardApp::TabletInputDeviceUIData>()) {
    return mTabletDevice;
  }

  return mGenericDevice;
}

DataTemplate InputDeviceUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item,
  const DependencyObject&) {
  return this->SelectTemplateCore(item);
}

}// namespace winrt::OpenKneeboardApp::implementation
