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
#include "DegreesValueConverter.h"
#include "DegreesValueConverter.g.cpp"
// clang-format on

#include <fmt/format.h>

namespace winrt::OpenKneeboardApp::implementation {
winrt::Windows::Foundation::IInspectable DegreesValueConverter::Convert(
  winrt::Windows::Foundation::IInspectable const& value,
  winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
  winrt::Windows::Foundation::IInspectable const& parameter,
  hstring const& language) {
  return box_value(
    to_hstring(fmt::format("{}°", std::lround(unbox_value<double>(value)))));
}

winrt::Windows::Foundation::IInspectable DegreesValueConverter::ConvertBack(
  winrt::Windows::Foundation::IInspectable const& value,
  winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
  winrt::Windows::Foundation::IInspectable const& parameter,
  hstring const& language) {
  throw hresult_not_implemented();
}
}// namespace winrt::OpenKneeboardApp::implementation
