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

namespace OpenKneeboard::weak_refs_extensions {

template <class T>
struct make_weak_ref_fn {
  /** Create a weak ref from the specified value.
   *
   *  We have the class defined but useless so that we:
   *  - have the ability to specialize this class
   *  - keep SFINAE for invocation failures
   *
   * Ideally this would be a `static operator()`, but that:
   * - requires C++23
   * - is not yet widely supported as of 2024-08-05
   *
   * Declaring this method as `delete` has no effect; it's only here to
   * document what specializations need to implement.
   *
   * @returns a `weak_ref`
   */
  static constexpr auto make(T&&) = delete;
};

template <class T>
struct lock_weak_ref_fn {
  static constexpr auto lock(T&&) = delete;
};

}// namespace OpenKneeboard::weak_refs_extensions