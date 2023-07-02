/*
 * OpenKneeboard
 *
 * Copyright (C) 2023 Fred Emmott <fred@fredemmott.com>
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
#include "BooleanToVisibilityConverter.h"
#include "BooleanToVisibilityConverter.g.cpp"
// clang-format on

#include <winrt/Microsoft.UI.Xaml.h>

using Visibility = winrt::Microsoft::UI::Xaml::Visibility;

namespace winrt::OpenKneeboardApp::implementation {
winrt::Windows::Foundation::IInspectable BooleanToVisibilityConverter::Convert(
  winrt::Windows::Foundation::IInspectable const& value,
  winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
  winrt::Windows::Foundation::IInspectable const& parameter,
  hstring const& language) {
  return box_value(
    unbox_value<bool>(value) ? Visibility::Visible : Visibility::Collapsed);
}

winrt::Windows::Foundation::IInspectable
BooleanToVisibilityConverter::ConvertBack(
  winrt::Windows::Foundation::IInspectable const& value,
  winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
  winrt::Windows::Foundation::IInspectable const& parameter,
  hstring const& language) {
  throw hresult_not_implemented();
}
}// namespace winrt::OpenKneeboardApp::implementation
