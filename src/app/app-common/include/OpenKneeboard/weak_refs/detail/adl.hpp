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

namespace OpenKneeboard::weak_refs_adl_detail {

/* These are functions so that we get SFINAE; they are never defined,
 * as all we need is `decltype()` for the return value to get the
 * information we need.
 */
template <class T, class = decltype(make_weak_ref(std::declval<T>()))>
consteval std::true_type is_adl_strong_ref_fn(T);
consteval std::false_type is_adl_strong_ref_fn(...);
template <class T>
concept adl_strong_ref
  = decltype(is_adl_strong_ref_fn(std::declval<T>()))::value;

template <class T, class = decltype(lock_weak_ref(std::declval<T>()))>
consteval std::true_type is_adl_sweak_ref_fn(T);
consteval std::false_type is_adl_weak_ref_fn(...);
template <class T>
concept adl_weak_ref = decltype(is_adl_weak_ref_fn(std::declval<T>()))::value;

template <class A, class B>
concept decays_to_same_as = std::same_as<std::decay_t<A>, std::decay_t<B>>;
}// namespace OpenKneeboard::weak_refs_adl_detail
