// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#pragma once
#include "MetersNumberFormatter.g.h"
// clang-format on

namespace winrt::OpenKneeboardApp::implementation {
struct MetersNumberFormatter : MetersNumberFormatterT<MetersNumberFormatter> {
  MetersNumberFormatter() = default;

  hstring FormatInt(int64_t value);
  hstring FormatUInt(uint64_t value);
  hstring FormatDouble(double value);

  winrt::Windows::Foundation::IReference<int64_t> ParseInt(hstring const& text);
  winrt::Windows::Foundation::IReference<uint64_t> ParseUInt(
    hstring const& text);
  winrt::Windows::Foundation::IReference<double> ParseDouble(
    hstring const& text);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct MetersNumberFormatter : MetersNumberFormatterT<
                                 MetersNumberFormatter,
                                 implementation::MetersNumberFormatter> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
