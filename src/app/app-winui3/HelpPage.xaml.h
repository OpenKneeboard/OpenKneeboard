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
// clang-format on

#include <OpenKneeboard/Events.h>

#include <string>

#include "HelpPage.g.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct HelpPage : HelpPageT<HelpPage>, private OpenKneeboard::EventReceiver {
  HelpPage();
  ~HelpPage();

  void OnCopyVersionDataClick(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  void OnCopyGameEventsClick(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  void OnCopyDPrintClick(const IInspectable&, const RoutedEventArgs&) noexcept;

  void OnDPrintLayoutChanged(const IInspectable&, const IInspectable&) noexcept;

  winrt::fire_and_forget OnExportClick(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  winrt::fire_and_forget OnCheckForUpdatesClick(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  void OnSelectedLicenseChanged(
    const IInspectable&,
    const Controls::SelectionChangedEventArgs&) noexcept;

 private:
  winrt::apartment_context mUIThread;
  std::string mVersionClipboardData;
  std::string mGameEventsClipboardData;
  std::wstring mDPrintClipboardData;
  bool mWasDPrintExpanded = false;

  void PopulateVersion();
  winrt::fire_and_forget PopulateEvents() noexcept;
  winrt::fire_and_forget PopulateDPrint() noexcept;
  void PopulateLicenses() noexcept;

  void ScrollDPrintToEnd();
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct HelpPage : HelpPageT<HelpPage, implementation::HelpPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
