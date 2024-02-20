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
// clang-format off
#include "pch.h"
#include "WindowPickerDialog.xaml.h"
#include "WindowPickerDialog.g.cpp"
#include "WindowPickerUIData.g.cpp"
// clang-format on

#include "ExecutableIconFactory.h"

#include <OpenKneeboard/WindowCaptureTab.h>

using namespace winrt::Windows::Foundation::Collections;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

WindowPickerDialog::WindowPickerDialog() {
  InitializeComponent();

  ExecutableIconFactory iconFactory;

  IVector<IInspectable> winrtWindows {
    winrt::single_threaded_vector<IInspectable>()};
  for (const auto& [hwnd, spec]: WindowCaptureTab::GetTopLevelWindows()) {
    OpenKneeboardApp::WindowPickerUIData uiData;
    uiData.Hwnd(reinterpret_cast<uint64_t>(hwnd));
    uiData.Title(to_hstring(spec.mTitle));
    uiData.Path(spec.mExecutableLastSeenPath.wstring());
    uiData.Icon(iconFactory.CreateXamlBitmapSource(
      spec.mExecutableLastSeenPath.wstring()));
    winrtWindows.Append(uiData);
  }

  List().ItemsSource(winrtWindows);
}

WindowPickerDialog::~WindowPickerDialog() = default;

void WindowPickerDialog::OnListSelectionChanged(
  const IInspectable&,
  const SelectionChangedEventArgs& args) noexcept {
  if (args.AddedItems().Size() == 0) {
    mHwnd = {};
    this->IsPrimaryButtonEnabled(false);
  } else {
    auto selected
      = args.AddedItems().GetAt(0).as<OpenKneeboardApp::WindowPickerUIData>();
    mHwnd = selected.Hwnd();
    this->IsPrimaryButtonEnabled(true);
  }
}

uint64_t WindowPickerDialog::Hwnd() {
  return mHwnd;
}

WindowPickerUIData::WindowPickerUIData() = default;

BitmapSource WindowPickerUIData::Icon() {
  return mIcon;
}

void WindowPickerUIData::Icon(const BitmapSource& value) {
  mIcon = value;
}

uint64_t WindowPickerUIData::Hwnd() {
  return mHwnd;
}

void WindowPickerUIData::Hwnd(uint64_t value) {
  mHwnd = value;
}

hstring WindowPickerUIData::Title() {
  return mTitle;
}

void WindowPickerUIData::Title(const hstring& value) {
  mTitle = value;
}

hstring WindowPickerUIData::Path() {
  return mPath;
}

void WindowPickerUIData::Path(const hstring& value) {
  mPath = value;
}

}// namespace winrt::OpenKneeboardApp::implementation
