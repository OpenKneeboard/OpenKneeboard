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
