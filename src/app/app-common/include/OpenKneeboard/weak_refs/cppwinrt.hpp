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

#include "../cppwinrt/concepts.hpp"
#include "base.hpp"

#include <winrt/Windows.Foundation.h>

#include <memory>

namespace OpenKneeboard::weak_refs_extensions {

using namespace OpenKneeboard::cppwinrt::cppwinrt_concepts;

template <cppwinrt_strong_ref T>
struct make_weak_ref_fn<T> {
  template <class TValue>
  static constexpr auto make(TValue&& value) {
    return winrt::make_weak(std::forward<TValue>(value));
  }
};

template <cppwinrt_weak_ref T>
struct lock_weak_ref_fn<T> {
  static constexpr auto lock(auto&& value) {
    return value.get();
  }
};

}// namespace OpenKneeboard::weak_refs_extensions

namespace OpenKneeboard::weak_refs {
static_assert(convertible_to_weak_ref<winrt::Windows::Foundation::IStringable>);
static_assert(strong_ref<winrt::Windows::Foundation::IStringable>);
static_assert(!weak_ref<winrt::Windows::Foundation::IStringable>);

static_assert(!strong_ref<decltype(winrt::make_weak(
                winrt::Windows::Foundation::IStringable {nullptr}))>);
static_assert(!strong_ref<winrt::Windows::Foundation::IStringable*>);
static_assert(weak_ref<decltype(winrt::make_weak(
                winrt::Windows::Foundation::IStringable {nullptr}))>);
}// namespace OpenKneeboard::weak_refs