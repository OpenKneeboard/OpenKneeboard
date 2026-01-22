// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
