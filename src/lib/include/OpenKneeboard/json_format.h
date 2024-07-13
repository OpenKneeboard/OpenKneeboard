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
#pragma once

#include <shims/nlohmann/json.hpp>
#include <shims/winrt/base.h>

#include <format>

template <>
struct std::formatter<nlohmann::json, char>
  : std::formatter<std::string, char> {
  auto format(const nlohmann::json& value, auto& formatContext) const {
    return std::formatter<std::string, char>::format(
      value.dump(2), formatContext);
  }
};

template <>
struct std::formatter<nlohmann::json, wchar_t>
  : std::formatter<winrt::hstring, wchar_t> {
  auto format(const nlohmann::json& value, auto& formatContext) const {
    return std::formatter<winrt::hstring, wchar_t>::format(
      winrt::to_hstring(value.dump(2)), formatContext);
  }
};