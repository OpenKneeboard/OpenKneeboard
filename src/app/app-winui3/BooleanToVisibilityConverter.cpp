// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
  winrt::Windows::UI::Xaml::Interop::TypeName const& /*targetType*/,
  winrt::Windows::Foundation::IInspectable const& /*parameter*/,
  hstring const& /*language*/) {
  return box_value(
    unbox_value<bool>(value) ? Visibility::Visible : Visibility::Collapsed);
}

winrt::Windows::Foundation::IInspectable
BooleanToVisibilityConverter::ConvertBack(
  winrt::Windows::Foundation::IInspectable const& /*value*/,
  winrt::Windows::UI::Xaml::Interop::TypeName const& /*targetType*/,
  winrt::Windows::Foundation::IInspectable const& /*parameter*/,
  hstring const& /*language*/) {
  throw hresult_not_implemented();
}
}// namespace winrt::OpenKneeboardApp::implementation
