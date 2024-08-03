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

#include "../cppwinrt.hpp"

namespace OpenKneeboard::weak_refs_adl_detail {
template <class T>
concept raw_pointer = std::is_pointer_v<T>;

template <raw_pointer T, class TWeak = decltype(std::declval<T>()->get_weak())>
consteval std::true_type is_cppwinrt_raw_pointer_fn(T&&);
consteval std::false_type is_cppwinrt_raw_pointer_fn(...);
}// namespace OpenKneeboard::weak_refs_adl_detail

namespace OpenKneeboard::weak_refs_adl_definitions {
template <class T>
concept cppwinrt_raw_pointer
  = decltype(weak_refs_adl_detail::is_cppwinrt_raw_pointer_fn(
    std::declval<T>()))::value;

template <cppwinrt_raw_pointer T>
struct adl_make_weak_ref<T> {
  static constexpr auto make(auto&& value) {
    return value->get_weak();
  }
};

// FIXME: test winrt::com_ptr()

}// namespace OpenKneeboard::weak_refs_adl_definitions