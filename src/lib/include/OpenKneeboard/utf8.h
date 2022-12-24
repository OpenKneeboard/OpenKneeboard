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

#include <concepts>
#include <functional>
#include <shims/filesystem>
#include <string>
#include <type_traits>

/** This file exists to workaround the fact that `std::u8string`
 * and `char8_t` are not widely supported as of early 2022.
 *
 * Hopefully it can be removed in the future as library support
 * improves.
 */

namespace OpenKneeboard {

/// Translation marker which requires a literal string
#ifdef _
#undef _
#endif
template <class T, size_t N>
  requires std::same_as<T, typename std::char_traits<T>::char_type>
constexpr auto _(const T (&str)[N]) {
  return str;
}

// Allow construction of std::string and std::string_view
constexpr std::string to_utf8(const std::string& in) {
  return in;
}
constexpr std::string_view to_utf8(std::string_view in) {
  return in;
}
constexpr std::string_view to_utf8(const char* in) {
  return in;
}

std::string to_utf8(const std::wstring&);
std::string to_utf8(std::wstring_view);
std::string to_utf8(const wchar_t*);
std::string to_utf8(const std::filesystem::path&);

struct utf8_string : std::basic_string<char> {
  constexpr utf8_string() = default;

  template <class T>
  utf8_string(T&& x) : std::basic_string<char>(to_utf8(std::forward<T>(x))) {
  }

  inline operator std::filesystem::path() const noexcept {
    const auto first = reinterpret_cast<const char8_t*>(data());
    return {first, first + size()};
  }
};

}// namespace OpenKneeboard
#include <nlohmann/json.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN

template <>
struct adl_serializer<std::filesystem::path> {
  static void to_json(nlohmann::json&, const std::filesystem::path&);
  static void from_json(const nlohmann::json&, std::filesystem::path&);
};

template <>
struct adl_serializer<std::wstring> {
  static void from_json(const nlohmann::json&, std::wstring&) = delete;
  static void to_json(nlohmann::json&, const std::wstring&) = delete;
};

template <>
struct adl_serializer<std::wstring_view> {
  static void from_json(const nlohmann::json&, std::wstring_view&) = delete;
  static void to_json(nlohmann::json&, const std::wstring_view&) = delete;
};
template <>
struct adl_serializer<wchar_t*> {
  static void from_json(const nlohmann::json&, wchar_t*) = delete;
  static void to_json(nlohmann::json&, const wchar_t*) = delete;
};

NLOHMANN_JSON_NAMESPACE_END
