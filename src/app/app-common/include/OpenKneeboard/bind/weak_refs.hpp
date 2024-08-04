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

#include "../bind/bind_front.hpp"
#include "../weak_refs/bind_maybe_refs_front.hpp"
#include "../weak_refs/bind_refs_front.hpp"

namespace OpenKneeboard::bind_detail {

// Markers for tag-based dispatch
struct maybe_refs_t {};
struct only_refs_t {};

template <class Fn, class... Args>
constexpr auto adl_bind_front(maybe_refs_t, Fn&& fn, Args&&... args) {
  return weak_refs::bind_maybe_refs_front(
    std::forward<Fn>(fn), std::forward<Args>(args)...);
}

template <class Fn, class... Args>
constexpr auto adl_bind_front(only_refs_t, Fn&& fn, Args&&... args) {
  return weak_refs::bind_refs_front(
    std::forward<Fn>(fn), std::forward<Args>(args)...);
}
}// namespace OpenKneeboard::bind_detail

namespace OpenKneeboard::bind::inline weak_refs_bind_tags {

// Markers for tag-based dispatch
constexpr bind_detail::maybe_refs_t maybe_refs {};
constexpr bind_detail::only_refs_t only_refs {};

}// namespace OpenKneeboard::bind