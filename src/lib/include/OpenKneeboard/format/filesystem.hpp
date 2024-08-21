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
