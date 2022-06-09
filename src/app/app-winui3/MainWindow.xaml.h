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

#include <OpenKneeboard/Events.h>

#include <memory>
#include <thread>

#include "MainWindow.g.h"

using namespace winrt::Microsoft::UI::Dispatching;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Navigation;

namespace winrt::OpenKneeboardApp::implementation {
struct MainWindow : MainWindowT<MainWindow>, OpenKneeboard::EventReceiver {
  MainWindow();
  ~MainWindow();

  void OnNavigationItemInvoked(
    const IInspectable& sender,
    const NavigationViewItemInvokedEventArgs& args) noexcept;

  void OnNavigated(
    const IInspectable& sender,
    const NavigationEventArgs& args) noexcept;

  void OnBackRequested(
    const IInspectable& sender,
    const NavigationViewBackRequestedEventArgs&) noexcept;

  void UpdateTitleBarMargins(
    const IInspectable& sender,
    const IInspectable& args) noexcept;

 private:
  HWND mHwnd;
  winrt::handle mHwndFile;
  NavigationViewItem mTabSettingsItem;
  NavigationViewItem mGameSettingsItem;
  NavigationViewItem mBindingSettingsItem;

  DispatcherQueueController mDQC {nullptr};
  DispatcherQueueTimer mFrameTimer {nullptr};

  void OnTabChanged();
  void OnTabsChanged();

  void SaveWindowPosition();

  winrt::Windows::Foundation::IAsyncAction OnClosed(
    const IInspectable&,
    const WindowEventArgs&) noexcept;
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
