// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "WindowPickerDialog.xaml.h"
#include "WindowPickerDialog.g.cpp"
#include "WindowPickerUIData.g.cpp"
// clang-format on

#include "ExecutableIconFactory.h"

#include <OpenKneeboard/WindowCaptureTab.hpp>

#include <string_view>

using namespace winrt::Windows::Foundation::Collections;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

WindowPickerDialog::WindowPickerDialog() {
  InitializeComponent();

  ExecutableIconFactory iconFactory;

  std::vector<IInspectable> winrtWindows;
  for (const auto& [hwnd, spec]: WindowCaptureTab::GetTopLevelWindows()) {
    OpenKneeboardApp::WindowPickerUIData uiData;
    uiData.Hwnd(reinterpret_cast<uint64_t>(hwnd));
    uiData.Title(to_hstring(spec.mTitle));
    uiData.Path(spec.mExecutableLastSeenPath.wstring());
    uiData.Icon(iconFactory.CreateXamlBitmapSource(
      spec.mExecutableLastSeenPath.wstring()));
    winrtWindows.push_back(uiData);
  }

  mWindows = winrtWindows;
  List().ItemsSource(
    single_threaded_vector<IInspectable>(std::move(winrtWindows)));
}

WindowPickerDialog::~WindowPickerDialog() = default;

void WindowPickerDialog::OnListSelectionChanged(
  const IInspectable&,
  const SelectionChangedEventArgs& args) noexcept {
  if (args.AddedItems().Size() == 0) {
    mHwnd = {};
    this->IsPrimaryButtonEnabled(false);
  } else {
    auto selected =
      args.AddedItems().GetAt(0).as<OpenKneeboardApp::WindowPickerUIData>();
    mHwnd = selected.Hwnd();
    this->IsPrimaryButtonEnabled(true);
  }
}

uint64_t WindowPickerDialog::Hwnd() { return mHwnd; }

void WindowPickerDialog::OnAutoSuggestTextChanged(
  const AutoSuggestBox& box,
  const AutoSuggestBoxTextChangedEventArgs& args) noexcept {
  if (args.Reason() != AutoSuggestionBoxTextChangeReason::UserInput) {
    return;
  }

  const std::wstring_view queryText {box.Text()};
  if (queryText.empty()) {
    box.ItemsSource({nullptr});
    if (mFiltered) {
      List().ItemsSource(single_threaded_vector(
        std::vector<IInspectable> {mWindows.begin(), mWindows.end()}));
      mFiltered = false;
    }
    return;
  }

  auto matching = this->GetFilteredWindows(queryText);

  std::ranges::sort(matching, {}, [](const auto& inspectable) {
    return fold_utf8(
      to_string(inspectable.as<OpenKneeboardApp::WindowPickerUIData>()
                  .GetStringRepresentation()));
  });

  box.ItemsSource(single_threaded_vector(std::move(matching)));
}

std::vector<IInspectable> WindowPickerDialog::GetFilteredWindows(
  std::wstring_view queryText) {
  if (queryText.empty()) {
    return mWindows;
  }

  const auto folded = fold_utf8(to_utf8(queryText));
  auto words = std::string_view {folded}
    | std::views::split(std::string_view {" "})
    | std::views::transform([](auto subRange) {
                 return std::string_view {subRange.begin(), subRange.end()};
               });

  std::vector<IInspectable> ret;
  for (auto rawData: mWindows) {
    auto window = rawData.as<OpenKneeboardApp::WindowPickerUIData>();
    const auto title = fold_utf8(winrt::to_string(window.Title()));
    const auto path = fold_utf8(winrt::to_string(window.Path()));

    bool matchedAllWords = true;
    for (auto word: words) {
      if (title.find(word) != title.npos) {
        continue;
      }
      if (path.find(word) != path.npos) {
        continue;
      }
      matchedAllWords = false;
      break;
    }
    if (matchedAllWords) {
      ret.push_back(window);
    }
  }

  return ret;
}

void WindowPickerDialog::OnAutoSuggestQuerySubmitted(
  const AutoSuggestBox&,
  const AutoSuggestBoxQuerySubmittedEventArgs& args) noexcept {
  if (auto it = args.ChosenSuggestion()) {
    List().ItemsSource(single_threaded_vector<IInspectable>({it}));
    List().SelectedItem(it);
    return;
  }

  List().ItemsSource(
    single_threaded_vector(this->GetFilteredWindows(args.QueryText())));

  mFiltered = true;
}

WindowPickerUIData::WindowPickerUIData() = default;

BitmapSource WindowPickerUIData::Icon() { return mIcon; }

void WindowPickerUIData::Icon(const BitmapSource& value) { mIcon = value; }

uint64_t WindowPickerUIData::Hwnd() { return mHwnd; }

void WindowPickerUIData::Hwnd(uint64_t value) { mHwnd = value; }

hstring WindowPickerUIData::Title() { return mTitle; }

void WindowPickerUIData::Title(const hstring& value) { mTitle = value; }

hstring WindowPickerUIData::Path() { return mPath; }

void WindowPickerUIData::Path(const hstring& value) { mPath = value; }

hstring WindowPickerUIData::GetStringRepresentation() {
  return hstring {std::format(
    L"{} ({})",
    std::wstring_view {Title()},
    std::filesystem::path(std::wstring_view {Path()}).filename().wstring())};
}

}// namespace winrt::OpenKneeboardApp::implementation
