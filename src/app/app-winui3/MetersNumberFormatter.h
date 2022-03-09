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
  winrt::Windows::Foundation::IReference<uint64_t> ParseUInt(hstring const& text);
  winrt::Windows::Foundation::IReference<double> ParseDouble(hstring const& text);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct MetersNumberFormatter : MetersNumberFormatterT<
                                 MetersNumberFormatter,
                                 implementation::MetersNumberFormatter> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
