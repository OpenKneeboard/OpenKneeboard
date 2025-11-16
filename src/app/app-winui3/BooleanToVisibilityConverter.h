// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "BooleanToVisibilityConverter.g.h"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {
struct BooleanToVisibilityConverter
  : BooleanToVisibilityConverterT<BooleanToVisibilityConverter> {
  BooleanToVisibilityConverter() = default;

  winrt::Windows::Foundation::IInspectable Convert(
    winrt::Windows::Foundation::IInspectable const& value,
    winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
    winrt::Windows::Foundation::IInspectable const& parameter,
    hstring const& language);
  winrt::Windows::Foundation::IInspectable ConvertBack(
    winrt::Windows::Foundation::IInspectable const& value,
    winrt::Windows::UI::Xaml::Interop::TypeName const& targetType,
    winrt::Windows::Foundation::IInspectable const& parameter,
    hstring const& language);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct BooleanToVisibilityConverter
  : BooleanToVisibilityConverterT<
      BooleanToVisibilityConverter,
      implementation::BooleanToVisibilityConverter> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
