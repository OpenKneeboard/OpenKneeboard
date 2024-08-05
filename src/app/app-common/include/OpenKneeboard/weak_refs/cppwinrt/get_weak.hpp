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

#include "../../cppwinrt/concepts.hpp"
#include "../cppwinrt.hpp"

namespace OpenKneeboard::weak_refs_extensions {

using namespace OpenKneeboard::cppwinrt::cppwinrt_concepts;

template <cppwinrt_raw_pointer T>
struct make_weak_ref_fn<T> {
  static constexpr auto make(auto&& value) {
    return value->get_weak();
  }
};

// FIXME: test winrt::com_ptr()

}// namespace OpenKneeboard::weak_refs_extensions