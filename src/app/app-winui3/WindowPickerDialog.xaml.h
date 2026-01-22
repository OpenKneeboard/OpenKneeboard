// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "WindowPickerDialog.g.h"
#include "WindowPickerUIData.g.h"
// clang-format on

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Foundation;

namespace winrt::OpenKneeboardApp::implementation {
struct WindowPickerDialog : WindowPickerDialogT<WindowPickerDialog> {
  WindowPickerDialog();
  ~WindowPickerDialog();

  uint64_t Hwnd();

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
  uint64_t mHwnd {};
  std::vector<IInspectable> mWindows;
  bool mFiltered {false};

  std::vector<IInspectable> GetFilteredWindows(std::wstring_view queryText);
};

struct WindowPickerUIData : WindowPickerUIDataT<WindowPickerUIData> {
  WindowPickerUIData();

  BitmapSource Icon();
  void Icon(const BitmapSource&);
  hstring Title();
  void Title(const hstring&);
  hstring Path();
  void Path(const hstring&);
  uint64_t Hwnd();
  void Hwnd(uint64_t);

  // ICustomPropertyProvider

  auto Type() const {
    return xaml_typename<OpenKneeboardApp::WindowPickerUIData>();
  }

  auto GetCustomProperty(const auto&) { return ICustomProperty {nullptr}; }

  auto GetIndexedProperty(const auto&, const auto&) {
    return ICustomProperty {nullptr};
  }

  hstring GetStringRepresentation();

 private:
  BitmapSource mIcon {nullptr};
  hstring mTitle;
  hstring mPath;
  uint64_t mHwnd {};
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct WindowPickerDialog : WindowPickerDialogT<
                              WindowPickerDialog,
                              implementation::WindowPickerDialog> {};

struct WindowPickerUIData : WindowPickerUIDataT<
                              WindowPickerUIData,
                              implementation::WindowPickerUIData> {};

}// namespace winrt::OpenKneeboardApp::factory_implementation
