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
#include "ProcessPickerDialog.g.h"
// clang-format on

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
struct ProcessPickerDialog : ProcessPickerDialogT<ProcessPickerDialog> {
  ProcessPickerDialog();
  ~ProcessPickerDialog();

  hstring SelectedPath() noexcept;
  bool GamesOnly() const noexcept;
  void GamesOnly(bool) noexcept;

  void OnListSelectionChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&) noexcept;

  void OnAutoSuggestTextChanged(
    const AutoSuggestBox&,
    const AutoSuggestBoxTextChangedEventArgs&) noexcept;

  void OnAutoSuggestQuerySubmitted(
    const AutoSuggestBox&,
    const AutoSuggestBoxQuerySubmittedEventArgs&) noexcept;

 private:
  hstring mSelectedPath;
  std::vector<IInspectable> mProcesses;
  bool mFiltered {false};
  bool mGamesOnly {true};

  std::vector<IInspectable> GetFilteredProcesses(std::wstring_view queryText);
  void Reload();
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct ProcessPickerDialog : ProcessPickerDialogT<
                               ProcessPickerDialog,
                               implementation::ProcessPickerDialog> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
