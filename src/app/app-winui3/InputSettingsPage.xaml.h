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
#include "InputSettingsPage.g.h"
// clang-format on

#include <string>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenKneeboardApp::implementation {
struct InputSettingsPage : InputSettingsPageT<InputSettingsPage> {
  InputSettingsPage();
  IVector<IInspectable> Devices();
  void OnOrientationChanged(const IInspectable&, const SelectionChangedEventArgs&);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct InputSettingsPage
  : InputSettingsPageT<InputSettingsPage, implementation::InputSettingsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
