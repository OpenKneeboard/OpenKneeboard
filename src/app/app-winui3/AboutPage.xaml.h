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

#include "AboutPage.g.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenKneeboardApp::implementation {
struct AboutPage : AboutPageT<AboutPage>, private OpenKneeboard::EventReceiver {
  AboutPage();

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

 private:
  winrt::apartment_context mUIThread;
  std::string mVersionClipboardData;
  std::string mGameEventsClipboardData;
  std::wstring mDPrintClipboardData;

  void PopulateVersion();
  void PopulateEvents();
  void PopulateDPrint();

  void ScrollDPrintToEnd();
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
