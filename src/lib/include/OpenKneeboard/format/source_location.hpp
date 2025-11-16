// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <format>
#include <source_location>

template <class CharT>
struct std::formatter<std::source_location, CharT>
  : std::formatter<std::basic_string_view<CharT>, CharT> {
  template <class FormatContext>
  auto format(const std::source_location& loc, FormatContext& fc) const {
    const auto converted = std::format(
      "{}:{}:{} - {}",
      loc.file_name(),
      loc.line(),
      loc.column(),
      loc.function_name());

    return std::formatter<std::basic_string_view<CharT>, CharT>::format(
      std::basic_string_view<CharT> {converted}, fc);
  }
};