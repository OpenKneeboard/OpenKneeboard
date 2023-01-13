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
#include "TabSettingsPage.g.h"

#include "TabUIData.g.h"
#include "DCSRadioLogTabUIData.g.h"
#include "WindowCaptureTabUIData.g.h"

#include "TabUIDataTemplateSelector.g.h"
// clang-format on

#include <OpenKneeboard/Events.h>

#include <string>

#include "WithPropertyChangedEvent.h"

namespace OpenKneeboard {
class ITab;
class ITabView;
class DCSRadioLogTab;
class WindowCaptureTab;
enum class TabType;
}// namespace OpenKneeboard

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenKneeboardApp::implementation {
struct TabSettingsPage : TabSettingsPageT<TabSettingsPage>,
                         OpenKneeboard::EventReceiver,
                         OpenKneeboard::WithPropertyChangedEvent {
  TabSettingsPage();
  ~TabSettingsPage() noexcept;

  IVector<IInspectable> Tabs() noexcept;

  fire_and_forget RestoreDefaults(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  void CreateTab(const IInspectable&, const RoutedEventArgs&) noexcept;
  fire_and_forget RemoveTab(const IInspectable&, const RoutedEventArgs&);
  fire_and_forget RenameTab(const IInspectable&, const RoutedEventArgs&);
  void OnTabsChanged(
    const IInspectable&,
    const Windows::Foundation::Collections::IVectorChangedEventArgs&) noexcept;

 private:
  void CreateAddTabMenu(const Button& button, FlyoutPlacementMode);

  template <class T>
  void CreateFileTab(const std::string& pickerDialogTitle = {});
  void CreateFolderTab();
  winrt::fire_and_forget CreateWindowCaptureTab();

  void AddTabs(const std::vector<std::shared_ptr<OpenKneeboard::ITab>>&);
  static OpenKneeboardApp::TabUIData CreateTabUIData(
    const std::shared_ptr<OpenKneeboard::ITab>&);

  bool mUIIsChangingTabs = false;
};

struct TabUIData : TabUIDataT<TabUIData>,
                   OpenKneeboard::EventReceiver,
                   OpenKneeboard::WithPropertyChangedEvent {
  TabUIData() = default;
  ~TabUIData();

  hstring Title() const;

  uint64_t InstanceID() const;
  void InstanceID(uint64_t);

 protected:
  std::weak_ptr<OpenKneeboard::ITab> mTab;
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
  std::shared_ptr<OpenKneeboard::DCSRadioLogTab> GetTab() const;
};

struct WindowCaptureTabUIData : WindowCaptureTabUIDataT<
                                  WindowCaptureTabUIData,
                                  OpenKneeboardApp::implementation::TabUIData> {
  WindowCaptureTabUIData() = default;

  hstring WindowTitle();
  void WindowTitle(hstring const& value);
  bool MatchWindowClass();
  void MatchWindowClass(bool value);
  uint8_t MatchWindowTitle();
  void MatchWindowTitle(uint8_t value);
  bool IsInputEnabled() const;
  void IsInputEnabled(bool value);

 private:
  std::shared_ptr<OpenKneeboard::WindowCaptureTab> GetTab() const;
};

struct TabUIDataTemplateSelector
  : TabUIDataTemplateSelectorT<TabUIDataTemplateSelector> {
  TabUIDataTemplateSelector() = default;

  winrt::Microsoft::UI::Xaml::DataTemplate Generic();
  void Generic(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate DCSRadioLog();
  void DCSRadioLog(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate WindowCapture();
  void WindowCapture(winrt::Microsoft::UI::Xaml::DataTemplate const& value);

  DataTemplate SelectTemplateCore(const IInspectable&);
  DataTemplate SelectTemplateCore(const IInspectable&, const DependencyObject&);

 private:
  DataTemplate mGeneric;
  DataTemplate mDCSRadioLog;
  DataTemplate mWindowCapture;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabSettingsPage
  : TabSettingsPageT<TabSettingsPage, implementation::TabSettingsPage> {};

struct TabUIData : TabUIDataT<TabUIData, implementation::TabUIData> {};
struct DCSRadioLogTabUIData : DCSRadioLogTabUIDataT<
                                DCSRadioLogTabUIData,
                                implementation::DCSRadioLogTabUIData> {};
struct WindowCaptureTabUIData : WindowCaptureTabUIDataT<
                                  WindowCaptureTabUIData,
                                  implementation::WindowCaptureTabUIData> {};

struct TabUIDataTemplateSelector
  : TabUIDataTemplateSelectorT<
      TabUIDataTemplateSelector,
      implementation::TabUIDataTemplateSelector> {};

}// namespace winrt::OpenKneeboardApp::factory_implementation
