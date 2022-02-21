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
#include <filesystem>
#include <string>
#include <type_traits>

class wxString;

/** This file exists to workaround the fact that `std::u8string`
 * and `char8_t` are not widely supported as of early 2022.
 *
 * Hopefully it can be removed in the future as library support
 * improves.
 */

namespace OpenKneeboard {

/// Translation marker
inline std::string TranslateString(std::string_view in) {
  return std::string(in);
}

#ifdef _
#undef _
#endif

#define _(x) (::OpenKneeboard::TranslateString(x))

class utf8_string_view;
class utf8_string;

// Allow construction of utf8_string and utf8_string_view
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
std::string to_utf8(const wxString&);

class utf8_string_view final {
 private:
  std::string mBuffer;
  std::string_view mView;

 public:
  constexpr utf8_string_view(std::string_view in) : mView(in) {
  }

  template <class T>
  utf8_string_view(T in) : mBuffer(to_utf8(in)), mView(mBuffer) {
  }

  operator std::filesystem::path() const;

  constexpr operator std::string_view() const {
    return mView;
  }
};

class utf8_string final : public std::string {
 public:
  using std::string::string;

  template <class T>
  utf8_string(T in) : std::string(to_utf8(in)) {
  }

  explicit constexpr utf8_string(utf8_string_view in) : std::string(in) {
  }

  constexpr operator utf8_string_view() const {
    return utf8_string_view(static_cast<std::string_view>(*this));
  }

  // clang-format off
  template <class T>
  requires (
    std::convertible_to<utf8_string_view, T>
    && !std::same_as<utf8_string_view, std::decay_t<T>>
  )
  // clang-format on
  operator T() const {
    return static_cast<utf8_string_view>(*this);
  }
};

}// namespace OpenKneeboard
