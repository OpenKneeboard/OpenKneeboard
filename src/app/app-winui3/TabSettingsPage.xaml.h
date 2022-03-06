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
#include "TabUIData.g.h"
#include "TabSettingsPage.g.h"
// clang-format on

#include <string>

namespace OpenKneeboard {
  class Tab;
  class TabState;
  enum class TabType;
}


using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct TabSettingsPage : TabSettingsPageT<TabSettingsPage> {
  TabSettingsPage();

  void CreateTab(const IInspectable&, const RoutedEventArgs&);
  fire_and_forget RemoveTab(const IInspectable&, const RoutedEventArgs&);
  void OnTabsChanged(const IInspectable&, const Windows::Foundation::Collections::IVectorChangedEventArgs&);
 private:
  template<class T>
  fire_and_forget CreateFileBasedTab(hstring filenameExtension);
  template<class T>
  fire_and_forget CreateFolderBasedTab();

  void AddTab(const std::shared_ptr<OpenKneeboard::Tab>&);
  std::string mData;
};

struct TabUIData : TabUIDataT<TabUIData> {
  TabUIData() = default;

  hstring Title();
  void Title(hstring);

  uint64_t InstanceID();
  void InstanceID(uint64_t);

 private:
  hstring mTitle;
  uint64_t mInstanceID {~0ui64};
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabSettingsPage
  : TabSettingsPageT<TabSettingsPage, implementation::TabSettingsPage> {};

struct TabUIData : TabUIDataT<TabUIData, implementation::TabUIData> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
