// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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
