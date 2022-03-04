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
#include "TabData.g.h"
#include "TabSettingsPage.g.h"
// clang-format on

#include <string>

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct TabSettingsPage : TabSettingsPageT<TabSettingsPage> {
  TabSettingsPage();

  fire_and_forget AddTab(const IInspectable&, const RoutedEventArgs&);
 private:
  std::string mData;
};

struct TabData : TabDataT<TabData> {
  TabData() = default;

  hstring Title();
  void Title(hstring);

  uint64_t UniqueID();
  void UniqueID(uint64_t);

 private:
  hstring mTitle;
  uint64_t mUniqueID {~0ui64};
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabSettingsPage
  : TabSettingsPageT<TabSettingsPage, implementation::TabSettingsPage> {};

struct TabData : TabDataT<TabData, implementation::TabData> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
