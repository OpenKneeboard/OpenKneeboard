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
  hstring const& text) {
  throw hresult_not_implemented();
}
winrt::Windows::Foundation::IReference<uint64_t>
MetersNumberFormatter::ParseUInt(hstring const& text) {
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
  } catch (std::invalid_argument) {
    return {};
  }
}

}// namespace winrt::OpenKneeboardApp::implementation
