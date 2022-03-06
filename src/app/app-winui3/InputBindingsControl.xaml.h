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
#include "InputBindingsControl.g.h"
// clang-format on

namespace OpenKneeboard {
  class UserInputDevice;
  enum class UserAction;
}

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
struct InputBindingsControl : InputBindingsControlT<InputBindingsControl> {
  InputBindingsControl();
  ~InputBindingsControl();

  hstring DeviceID();
  void DeviceID(const hstring&);

 private:
  void UpdateUI();
  void UpdateUI(
    UserAction,
    TextBlock label,
    Button bindButton,
    Button ClearButton
  );

  hstring mDeviceID;
  std::shared_ptr<UserInputDevice> mDevice;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct InputBindingsControl : InputBindingsControlT<
                                InputBindingsControl,
                                implementation::InputBindingsControl> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
