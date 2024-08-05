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

#include "extensions.hpp"

namespace OpenKneeboard::weak_refs::inline weak_refs_ref_types {

/// e.g. `std::weak_ptr<T>`
template <class TStrong>
using weak_ref_t
  = decltype(weak_refs_extensions::make_weak_ref_fn<
             std::decay_t<TStrong>>::make(std::declval<TStrong>()));
/// e.g. `std::strong_ptr<T>`
template <class TWeak>
using strong_ref_t
  = decltype(weak_refs_extensions::lock_weak_ref_fn<std::decay_t<TWeak>>::lock(
    std::declval<TWeak>()));

}// namespace OpenKneeboard::weak_refs::inline weak_refs_ref_types