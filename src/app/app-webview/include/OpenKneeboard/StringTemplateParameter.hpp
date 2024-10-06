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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#pragma once

#include <algorithm>
#include <ranges>
#include <string_view>
#include <type_traits>

namespace OpenKneeboard {
template <char... V>
struct StringTemplateParameter {
  static constexpr std::array<char, sizeof...(V)> value_v {V...};
  // Exclude null terminator
  static constexpr std::size_t size_v = value_v.size() - 1;

  static constexpr auto size() {
    return size_v;
  };

  constexpr operator std::string_view() const {
    return {value_v.data(), size_v};
  }

  template <char... VV>
  constexpr auto operator+(
    StringTemplateParameter<VV...> other) const noexcept {
    return []<std::size_t... I>(std::index_sequence<I...>) {
      return StringTemplateParameter<value_v.at(I)..., VV...> {};
    }(std::make_index_sequence<size_v> {});
  }

  template <char... VV>
  static constexpr bool starts_with(
    StringTemplateParameter<VV...> other) noexcept {
    if (size_v < other.size_v) {
      return false;
    }
    for (std::size_t i = 0; i < other.size_v; ++i) {
      if (value_v[i] != other.value_v[i]) {
        return false;
      }
    }
    return true;
  }

  template <char... VV>
  static consteval auto remove_prefix(
    StringTemplateParameter<VV...> other) noexcept {
    if constexpr (starts_with(other)) {
      constexpr auto offset = sizeof...(VV) - 1;
      constexpr auto new_size = sizeof...(V) - offset;
      return []<std::size_t... I>(std::index_sequence<I...>) {
        return StringTemplateParameter<value_v[I + offset]..., '\0'> {};
      }(std::make_index_sequence<new_size - 1>());
    } else {
      return StringTemplateParameter<V...> {};
    }
  }

  template <char... VV>
  constexpr bool operator==(StringTemplateParameter<VV...> other) noexcept {
    if (size_v != other.size_v) {
      return false;
    }
    return std::ranges::equal(value_v, other.value_v);
  }
};

template <std::size_t N>
struct StringTemplateParameterHelper {
  consteval StringTemplateParameterHelper(const char (&init)[N]) {
    std::ranges::copy(init, value);
  }

  char value[N];
  static constexpr std::size_t size_v = N;
};

// *T*emplate *P*aram
template <StringTemplateParameterHelper helper>
consteval auto operator""_tp() {
  return []<std::size_t... I>(std::index_sequence<I...>) {
    return StringTemplateParameter<helper.value[I]...> {};
  }(std::make_index_sequence<helper.size_v> {});
}

static_assert("foo"_tp.starts_with("f"_tp));
static_assert("foo"_tp.starts_with("foo"_tp));
static_assert(!"foo"_tp.starts_with("food"_tp));
static_assert("foo"_tp.remove_prefix("f"_tp) == "oo"_tp);
static_assert("foo"_tp.remove_prefix("goo"_tp) == "foo"_tp);
static_assert("foo"_tp.remove_prefix("foo"_tp) == ""_tp);
static_assert(
  "foo"
  "bar"
  ""_tp
  == "foobar"_tp);
static_assert(
  "foo"
  "bar"_tp
  ""
  == "foobar"_tp);

}// namespace OpenKneeboard
