// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <filesystem>

namespace OpenKneeboard {
std::string to_utf8(const std::filesystem::path&);
}

template <class CharT>
struct std::formatter<std::filesystem::path, CharT>
  : std::formatter<std::basic_string_view<CharT>, CharT> {
  template <class FormatContext>
  auto format(const std::filesystem::path& path, FormatContext& fc) const {
    std::basic_string<CharT> converted;
    if constexpr (std::same_as<CharT, char>) {
      converted = OpenKneeboard::to_utf8(path.wstring());
    } else {
      static_assert(std::same_as<CharT, wchar_t>);
      converted = path.wstring();
    }
    return std::formatter<std::basic_string_view<CharT>, CharT>::format(
      std::basic_string_view<CharT> {converted}, fc);
  }
};
