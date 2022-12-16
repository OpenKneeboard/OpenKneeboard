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
#include "WindowPickerDialog.g.h"
#include "WindowPickerUIData.g.h"
// clang-format on

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace winrt::OpenKneeboardApp::implementation {
struct WindowPickerDialog : WindowPickerDialogT<WindowPickerDialog> {
  WindowPickerDialog();
  ~WindowPickerDialog();

  uint64_t Hwnd();

  void OnListSelectionChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&) noexcept;
 private:
  uint64_t mHwnd {};
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
