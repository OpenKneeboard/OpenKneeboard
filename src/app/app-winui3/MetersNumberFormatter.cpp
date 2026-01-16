// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "MetersNumberFormatter.h"
#include "MetersNumberFormatter.g.cpp"
// clang-format on

#include <format>
#include <string>

namespace winrt::OpenKneeboardApp::implementation {
hstring MetersNumberFormatter::FormatInt(int64_t value) {
  return to_hstring(std::format("{}m", value));
}
hstring MetersNumberFormatter::FormatUInt(uint64_t value) {
  return to_hstring(std::format("{}m", value));
}
hstring MetersNumberFormatter::FormatDouble(double value) {
  return to_hstring(std::format("{:0.2f}m", value));
}

winrt::Windows::Foundation::IReference<int64_t> MetersNumberFormatter::ParseInt(
  hstring const&) {
  throw hresult_not_implemented();
}
winrt::Windows::Foundation::IReference<uint64_t>
MetersNumberFormatter::ParseUInt(hstring const&) {
  throw hresult_not_implemented();
}

winrt::Windows::Foundation::IReference<double>
MetersNumberFormatter::ParseDouble(hstring const& text) {
  const auto str = to_string(text);

  // Allow a floating-point number, optionally suffixed by 'm'

  try {
    size_t parsed = 0;
    const auto value = std::stod(str, &parsed);

    if (parsed == 0) {
      return {};
    }

    if (parsed == str.size()) {
      return {value};
    }

    if (parsed == (str.size() - 1) && str.back() == 'm') {
      return {value};
    }

    return {};
  } catch (const std::invalid_argument&) {
    return {};
  }
}

}// namespace winrt::OpenKneeboardApp::implementation
