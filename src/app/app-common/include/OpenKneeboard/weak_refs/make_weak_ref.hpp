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

#include "concepts.hpp"
#include "detail/static_const.hpp"

/**
 * This is implemented using the pattern described in
 * "Suggested Design For Customization Points" (https://wg21.link/N4381)
 *
 * For this case, it's particular important that functors don't participate
 * in ADL, to avoid recursive compile-time tests for wether or not
 * the function is usable.
 *
 * For example, if I make `weak_refs::make_weak_ref()` available in namespace
 * `Foo`, it shouldn't be used in ADL for `make_ref(Foo::Bar{})`.
 */

namespace OpenKneeboard::weak_refs_detail {
struct make_weak_ref_fn {
  template <weak_refs::weak_ref T>
  constexpr auto operator()(const T& weak) const {
    return weak;
  }

  template <weak_refs::convertible_to_weak_ref T>
    requires(!weak_refs::weak_ref<T>)
  constexpr auto operator()(T&& strong) const {
    return weak_refs_extensions::make_weak_ref_fn<std::decay_t<T>>::make(
      std::forward<T>(strong));
  }
};
}// namespace OpenKneeboard::weak_refs_detail

namespace OpenKneeboard::weak_refs {

namespace {
constexpr auto const& make_weak_ref
  = weak_refs_detail::static_const<weak_refs_detail::make_weak_ref_fn>;
}// namespace

}// namespace OpenKneeboard::weak_refs