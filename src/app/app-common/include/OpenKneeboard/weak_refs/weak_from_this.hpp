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

#include "base.hpp"

#include <memory>

namespace OpenKneeboard::weak_refs_extensions {
namespace weak_refs = ::OpenKneeboard::weak_refs;

template <class T>
concept with_weak_from_this = std::is_pointer_v<T> && requires(T x) {
  { x->weak_from_this() } -> weak_refs::weak_ref;
};

static_assert(!with_weak_from_this<std::shared_ptr<int>>);

template <with_weak_from_this T>
struct make_weak_ref_fn<T> {
  static constexpr auto make(auto&& value) {
    return value->weak_from_this();
  }
};

}// namespace OpenKneeboard::weak_refs_extensions

// TODO: split to test file

#include "std_shared_ptr.hpp"

namespace OpenKneeboard::weak_refs_test {
using namespace OpenKneeboard::weak_refs;
struct TestWeakFromThis
  : public std::enable_shared_from_this<TestWeakFromThis> {};

static_assert(
  std::same_as<weak_ref_t<TestWeakFromThis*>, std::weak_ptr<TestWeakFromThis>>);

static_assert(convertible_to_weak_ref<TestWeakFromThis*>);
static_assert(!strong_ref<TestWeakFromThis*>);

}// namespace OpenKneeboard::weak_refs_test