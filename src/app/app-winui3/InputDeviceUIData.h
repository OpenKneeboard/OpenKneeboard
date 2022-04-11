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

// clang-format off
#include "pch.h"
#include "InputDeviceUIData.g.h"
#include "TabletInputDeviceUIData.g.h"
#include "InputDeviceUIDataTemplateSelector.g.h"
// clang-format on

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct InputDeviceUIData : InputDeviceUIDataT<InputDeviceUIData> {
  InputDeviceUIData() = default;

  hstring Name();
  void Name(const hstring& value);
  hstring DeviceID();
  void DeviceID(const hstring& value);

 private:
  hstring mName;
  hstring mDeviceID;
};

struct TabletInputDeviceUIData
  : TabletInputDeviceUIDataT<
      TabletInputDeviceUIData,
      OpenKneeboardApp::implementation::InputDeviceUIData> {
  TabletInputDeviceUIData() = default;

  uint8_t Orientation();
  void Orientation(uint8_t value);

  bool FixedWriteRadius();
  void FixedWriteRadius(bool value);
  bool FixedEraseRadius();
  void FixedEraseRadius(bool value);
  float EraseRadius();
  void EraseRadius(float value);
  float WriteRadius();
  void WriteRadius(float value);

 private:
  uint8_t mOrientation {0};
  float mWriteRadius {15};
  float mEraseRadius {150};
  bool mFixedEraseRadius {true};
  bool mFixedWriteRadius {false};
};

struct InputDeviceUIDataTemplateSelector
  : InputDeviceUIDataTemplateSelectorT<InputDeviceUIDataTemplateSelector> {
  InputDeviceUIDataTemplateSelector() = default;

  DataTemplate GenericDevice();
  void GenericDevice(const DataTemplate&);
  DataTemplate TabletDevice();
  void TabletDevice(const DataTemplate&);

  DataTemplate SelectTemplateCore(const IInspectable&);
  DataTemplate SelectTemplateCore(const IInspectable&, const DependencyObject&);

 private:
  DataTemplate mGenericDevice;
  DataTemplate mTabletDevice;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct InputDeviceUIData
  : InputDeviceUIDataT<InputDeviceUIData, implementation::InputDeviceUIData> {};
struct TabletInputDeviceUIData : TabletInputDeviceUIDataT<
                                   TabletInputDeviceUIData,
                                   implementation::TabletInputDeviceUIData> {};
struct InputDeviceUIDataTemplateSelector
  : InputDeviceUIDataTemplateSelectorT<
      InputDeviceUIDataTemplateSelector,
      implementation::InputDeviceUIDataTemplateSelector> {};

}// namespace winrt::OpenKneeboardApp::factory_implementation
