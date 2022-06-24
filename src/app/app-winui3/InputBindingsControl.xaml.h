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

#include <unordered_map>

namespace OpenKneeboard {
class UserInputDevice;
enum class UserAction;
}// namespace OpenKneeboard

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
struct InputBindingsControl : InputBindingsControlT<InputBindingsControl> {
  InputBindingsControl() noexcept;
  ~InputBindingsControl();

  hstring DeviceID();
  void DeviceID(const hstring&);

 private:
  winrt::apartment_context mUIThread;
  fire_and_forget PromptForBinding(UserAction);
  void ClearBinding(UserAction);

  void PopulateUI();
  void AppendUIRow(UserAction, winrt::hstring label);
  void UpdateUI();

  hstring mDeviceID;
  std::shared_ptr<UserInputDevice> mDevice;

  struct Row {
    TextBlock mCurrentBinding;
    Button mBindButton;
    Button mClearButton;
  };
  std::unordered_map<UserAction, Row> mRows;
  void UpdateUI(UserAction);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct InputBindingsControl : InputBindingsControlT<
                                InputBindingsControl,
                                implementation::InputBindingsControl> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
