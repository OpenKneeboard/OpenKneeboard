// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "PercentValueConverter.h"
#include "PercentValueConverter.g.cpp"
// clang-format on

#include <format>

namespace winrt::OpenKneeboardApp::implementation {
winrt::Windows::Foundation::IInspectable PercentValueConverter::Convert(
  winrt::Windows::Foundation::IInspectable const& value,
  winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
  winrt::Windows::Foundation::IInspectable const& parameter,
  hstring const& language) {
  return box_value(
    to_hstring(std::format("{}%", std::lround(unbox_value<double>(value)))));
}

winrt::Windows::Foundation::IInspectable PercentValueConverter::ConvertBack(
  winrt::Windows::Foundation::IInspectable const& value,
  winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
  winrt::Windows::Foundation::IInspectable const& parameter,
  hstring const& language) {
  throw hresult_not_implemented();
}
}// namespace winrt::OpenKneeboardApp::implementation
