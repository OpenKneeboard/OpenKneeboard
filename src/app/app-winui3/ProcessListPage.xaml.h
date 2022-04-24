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
#include "ProcessListPage.g.h"
// clang-format on

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
struct ProcessListPage : ProcessListPageT<ProcessListPage> {
  ProcessListPage();
  ~ProcessListPage();

  hstring SelectedPath() noexcept;
  winrt::event_token SelectionChanged(
    winrt::Windows::Foundation::EventHandler<hstring> const& handler) noexcept;
  void SelectionChanged(winrt::event_token const& token) noexcept;

  void OnListSelectionChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&) noexcept;

 private:
  winrt::event<Windows::Foundation::EventHandler<hstring>>
    mSelectionChangedEvent;
  hstring mSelectedPath;
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct ProcessListPage
  : ProcessListPageT<ProcessListPage, implementation::ProcessListPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
