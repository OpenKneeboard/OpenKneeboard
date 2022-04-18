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

#include <fmt/format.h>

#include <regex>
#include <string>

namespace winrt::OpenKneeboardApp::implementation {
hstring MetersNumberFormatter::FormatInt(int64_t value) {
  return to_hstring(fmt::format("{}m", value));
}
hstring MetersNumberFormatter::FormatUInt(uint64_t value) {
  return to_hstring(fmt::format("{}m", value));
}
hstring MetersNumberFormatter::FormatDouble(double value) {
  return to_hstring(fmt::format("{:0.2f}m", value));
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
  auto str = to_string(text);
  std::smatch match;
  if (!std::regex_match(str, match, std::regex("^([0-9.]+)m?$"))) {
    return {};
  }
  return {std::stod(match[1].str())};
}

}// namespace winrt::OpenKneeboardApp::implementation
