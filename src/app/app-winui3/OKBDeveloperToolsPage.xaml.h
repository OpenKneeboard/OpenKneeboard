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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#pragma once
#include "OKBDeveloperToolsPage.g.h"

namespace winrt::OpenKneeboardApp::implementation {
struct OKBDeveloperToolsPage : OKBDeveloperToolsPageT<OKBDeveloperToolsPage> {
  OKBDeveloperToolsPage();
  ~OKBDeveloperToolsPage();

  inline bool PluginFileTypeInHKCU() const noexcept;
  void PluginFileTypeInHKCU(bool value) noexcept;

  void OnCopyAPIEventsClick(
    const IInspectable&,
    const Microsoft::UI::Xaml::RoutedEventArgs&) noexcept;
  void OnCopyDebugMessagesClick(
    const IInspectable&,
    const Microsoft::UI::Xaml::RoutedEventArgs&) noexcept;

  winrt::hstring AutoUpdateFakeCurrentVersion() noexcept;
  OpenKneeboard::fire_and_forget AutoUpdateFakeCurrentVersion(winrt::hstring);

  OpenKneeboard::fire_and_forget OnTriggerCrashClick(
    IInspectable,
    Microsoft::UI::Xaml::RoutedEventArgs);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct OKBDeveloperToolsPage : OKBDeveloperToolsPageT<
                                 OKBDeveloperToolsPage,
                                 implementation::OKBDeveloperToolsPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
