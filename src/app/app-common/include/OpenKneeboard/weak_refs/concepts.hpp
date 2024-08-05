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

#include "ref_types.hpp"

#include <concepts>

namespace OpenKneeboard::weak_refs::inline weak_refs_concepts {

/** An object without keep-alive semantics, but can be 'locked' to produce one
 * with keep-alive semantics.
 *
 * e.g. `std::weak_ptr`
 */
template <class T>
concept weak_ref = requires(T v) {
  {
    weak_refs_extensions::lock_weak_ref_fn<std::decay_t<T>>::lock(v)
  } -> std::copyable;
};

/** Either a weak_ref, or an object that can be converted to a `weak_ref`.
 *
 * For example, an object implementing `get_weak()` via
 * `std::enable_shared_from_this`
 */
template <class T>
concept convertible_to_weak_ref
  = weak_ref<T> || weak_ref<weak_ref_t<std::decay_t<T>>>;

/** A pointer-like object with 'keep-alive' semantics.
 *
 * e.g. `std::shared_ptr`
 *
 * It is possible for these to hold an invalid reference, e.g. `nullptr`
 */
template <class T>
concept strong_ref = convertible_to_weak_ref<T>
  && std::same_as<std::decay_t<T>, strong_ref_t<weak_ref_t<T>>>;

static_assert(!strong_ref<int*>);
static_assert(!weak_ref<int*>);

}// namespace OpenKneeboard::weak_refs