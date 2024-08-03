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

#include "../adl.hpp"
#include "../ref_types.hpp"

namespace OpenKneeboard::weak_refs_detail {

template <class T, class = weak_refs::weak_ref_t<T>>
consteval std::true_type can_convert_to_weak_ref_fn(T);
consteval std::false_type can_convert_to_weak_ref_fn(...);

template <
  class T,
  class = decltype(weak_refs_adl_definitions::adl_lock_weak_ref<
                   std::decay_t<T>>::lock(std::declval<T>()))>
consteval std::true_type is_weak_ref_fn(T);
consteval std::false_type is_weak_ref_fn(...);

}// namespace OpenKneeboard::weak_refs_detail