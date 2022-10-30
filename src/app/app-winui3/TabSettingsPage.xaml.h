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
#include "DCSRadioLogTabUIData.g.h"
#include "TabUIDataTemplateSelector.g.h"
// clang-format on

#include <string>

namespace OpenKneeboard {
class ITab;
class ITabView;
class DCSRadioLogTab;
enum class TabType;
}// namespace OpenKneeboard

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct TabSettingsPage : TabSettingsPageT<TabSettingsPage> {
  TabSettingsPage();

  void CreateTab(const IInspectable&, const RoutedEventArgs&) noexcept;
  fire_and_forget RemoveTab(const IInspectable&, const RoutedEventArgs&);
  void OnTabsChanged(
    const IInspectable&,
    const Windows::Foundation::Collections::IVectorChangedEventArgs&) noexcept;

 private:
  fire_and_forget CreateFileTab();
  fire_and_forget CreateFolderTab();

  void AddTabs(const std::vector<std::shared_ptr<OpenKneeboard::ITab>>&);
  static OpenKneeboardApp::TabUIData CreateTabUIData(
    const std::shared_ptr<OpenKneeboard::ITab>&);
};

struct TabUIData : TabUIDataT<TabUIData> {
  TabUIData() = default;

  hstring Title() const;
  void Title(hstring);

  uint64_t InstanceID() const;
  void InstanceID(uint64_t);

 private:
  hstring mTitle;
  uint64_t mInstanceID {~0ui64};
};

struct DCSRadioLogTabUIData : DCSRadioLogTabUIDataT<
                                DCSRadioLogTabUIData,
                                OpenKneeboardApp::implementation::TabUIData> {
  DCSRadioLogTabUIData() = default;
  uint8_t MissionStartBehavior() const;
  void MissionStartBehavior(uint8_t value);
  bool TimestampsEnabled() const;
  void TimestampsEnabled(bool value);

 private:
  mutable std::weak_ptr<OpenKneeboard::DCSRadioLogTab> mTab;

  std::shared_ptr<OpenKneeboard::DCSRadioLogTab> GetTab() const;
};

struct TabUIDataTemplateSelector
  : TabUIDataTemplateSelectorT<TabUIDataTemplateSelector> {
  TabUIDataTemplateSelector() = default;

  winrt::Microsoft::UI::Xaml::DataTemplate Generic();
  void Generic(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate DCSRadioLog();
  void DCSRadioLog(winrt::Microsoft::UI::Xaml::DataTemplate const& value);

  DataTemplate SelectTemplateCore(const IInspectable&);
  DataTemplate SelectTemplateCore(const IInspectable&, const DependencyObject&);

 private:
  DataTemplate mGeneric;
  DataTemplate mDCSRadioLog;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabSettingsPage
  : TabSettingsPageT<TabSettingsPage, implementation::TabSettingsPage> {};

struct TabUIData : TabUIDataT<TabUIData, implementation::TabUIData> {};
struct DCSRadioLogTabUIData : DCSRadioLogTabUIDataT<
                                DCSRadioLogTabUIData,
                                implementation::DCSRadioLogTabUIData> {};

struct TabUIDataTemplateSelector
  : TabUIDataTemplateSelectorT<
      TabUIDataTemplateSelector,
      implementation::TabUIDataTemplateSelector> {};

}// namespace winrt::OpenKneeboardApp::factory_implementation
