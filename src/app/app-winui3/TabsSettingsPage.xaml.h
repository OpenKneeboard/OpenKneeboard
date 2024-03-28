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
#include "TabsSettingsPage.g.h"

#include "TabUIData.g.h"
#include "BrowserTabUIData.g.h"
#include "DCSRadioLogTabUIData.g.h"
#include "WindowCaptureTabUIData.g.h"

#include "TabUIDataTemplateSelector.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.h>

#include <OpenKneeboard/audited_ptr.h>

#include <string>

namespace OpenKneeboard {
class ITab;
class TabView;
class BrowserTab;
class DCSRadioLogTab;
struct DXResources;
class KneeboardState;
class WindowCaptureTab;
enum class TabType;
}// namespace OpenKneeboard

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenKneeboardApp::implementation {
struct TabsSettingsPage : TabsSettingsPageT<TabsSettingsPage>,
                          OpenKneeboard::EventReceiver,
                          OpenKneeboard::WithPropertyChangedEvent {
  TabsSettingsPage();
  ~TabsSettingsPage() noexcept;

  IVector<IInspectable> Tabs() noexcept;

  fire_and_forget RestoreDefaults(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  void CreateTab(const IInspectable&, const RoutedEventArgs&) noexcept;
  fire_and_forget RemoveTab(const IInspectable&, const RoutedEventArgs&);
  fire_and_forget ShowTabSettings(const IInspectable&, const RoutedEventArgs&);
  fire_and_forget ShowDebugInfo(const IInspectable&, const RoutedEventArgs&);

  void OnTabsChanged(
    const IInspectable&,
    const Windows::Foundation::Collections::IVectorChangedEventArgs&) noexcept;
  void OnAddBrowserAddressTextChanged(
    const IInspectable&,
    const IInspectable&) noexcept;

 private:
  void CreateAddTabMenu(const Button& button, FlyoutPlacementMode);

  template <class T>
  void CreateFileTab(const std::string& pickerDialogTitle = {});
  void CreateFolderTab();
  winrt::fire_and_forget CreateWindowCaptureTab();
  fire_and_forget CreateBrowserTab();
  winrt::fire_and_forget PromptToInstallWebView2();
  winrt::guid GetFilePickerPersistenceGuid();

  void AddTabs(const std::vector<std::shared_ptr<OpenKneeboard::ITab>>&);
  static OpenKneeboardApp::TabUIData CreateTabUIData(
    const std::shared_ptr<OpenKneeboard::ITab>&);

  bool mUIIsChangingTabs = false;

  OpenKneeboard::audited_ptr<OpenKneeboard::DXResources> mDXR;
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;
};

struct TabUIData : TabUIDataT<TabUIData>,
                   OpenKneeboard::EventReceiver,
                   OpenKneeboard::WithPropertyChangedEvent {
  TabUIData() = default;
  ~TabUIData();

  uint64_t InstanceID() const;
  void InstanceID(uint64_t);

  hstring Title() const;
  void Title(hstring);

  bool HasDebugInformation() const;
  hstring DebugInformation() const;

 protected:
  std::weak_ptr<OpenKneeboard::ITab> mTab;
};

struct BrowserTabUIData : BrowserTabUIDataT<
                            BrowserTabUIData,
                            OpenKneeboardApp::implementation::TabUIData> {
  BrowserTabUIData() = default;

  bool IsSimHubIntegrationEnabled() const;
  void IsSimHubIntegrationEnabled(bool);

  bool IsBackgroundTransparent() const;
  void IsBackgroundTransparent(bool);

  bool IsDeveloperToolsWindowEnabled() const;
  void IsDeveloperToolsWindowEnabled(bool);

 private:
  std::shared_ptr<OpenKneeboard::BrowserTab> GetTab() const;
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
  bool IsCursorCaptureEnabled() const;
  void IsCursorCaptureEnabled(bool value);
  bool CaptureClientArea() const;
  void CaptureClientArea(bool value);
  hstring ExecutablePathPattern() const;
  void ExecutablePathPattern(hstring);
  hstring WindowClass() const;
  void WindowClass(hstring);

 private:
  std::shared_ptr<OpenKneeboard::WindowCaptureTab> GetTab() const;
};

struct TabUIDataTemplateSelector
  : TabUIDataTemplateSelectorT<TabUIDataTemplateSelector> {
  TabUIDataTemplateSelector() = default;

  winrt::Microsoft::UI::Xaml::DataTemplate Generic();
  void Generic(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate Browser();
  void Browser(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate DCSRadioLog();
  void DCSRadioLog(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate WindowCapture();
  void WindowCapture(winrt::Microsoft::UI::Xaml::DataTemplate const& value);

  DataTemplate SelectTemplateCore(const IInspectable&);
  DataTemplate SelectTemplateCore(const IInspectable&, const DependencyObject&);

 private:
  DataTemplate mGeneric;
  DataTemplate mBrowser;
  DataTemplate mDCSRadioLog;
  DataTemplate mWindowCapture;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabsSettingsPage
  : TabsSettingsPageT<TabsSettingsPage, implementation::TabsSettingsPage> {};

struct TabUIData : TabUIDataT<TabUIData, implementation::TabUIData> {};
struct BrowserTabUIData
  : BrowserTabUIDataT<BrowserTabUIData, implementation::BrowserTabUIData> {};
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
