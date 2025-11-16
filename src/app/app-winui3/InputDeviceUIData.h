// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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

 private:
  uint8_t mOrientation {0};
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
