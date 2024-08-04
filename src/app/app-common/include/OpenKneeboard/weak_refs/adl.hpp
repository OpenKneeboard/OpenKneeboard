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

#include "detail/adl.hpp"

#include <concepts>

namespace OpenKneeboard::weak_refs_adl_definitions {

template <class T>
struct adl_make_weak_ref {
  // We have `class =` on `T` here instead of constraining the class to:
  // - keep SFINAE behavior if it is false
  // - keep the ability to specialize this class
  template <
    class TValue,
    class = decltype(make_weak_ref(std::declval<std::decay_t<TValue>>()))>
    requires std::same_as<std::decay_t<TValue>, std::decay_t<T>>
  static constexpr auto make(TValue&& value) {
    return make_weak_ref(std::forward<TValue>(value));
  }
};

// Same pattern as adl_make_weak_ref
template <class T>
struct adl_lock_weak_ref {
  template <
    class TValue,
    class = decltype(lock_weak_ref(std::declval<std::decay_t<TValue>>()))>
    requires std::same_as<std::decay_t<TValue>, std::decay_t<T>>
  static constexpr auto lock(TValue&& value) {
    return lock_weak_ref(std::forward<TValue>(value));
  }
};

}// namespace OpenKneeboard::weak_refs_adl_definitions