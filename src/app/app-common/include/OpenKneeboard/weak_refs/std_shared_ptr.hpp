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
template <class T>
concept std_shared_ptr = std::same_as<
  std::decay_t<T>,
  std::shared_ptr<std::decay_t<typename T::element_type>>>;

template <class T>
concept std_weak_ptr = std::same_as<
  std::decay_t<T>,
  std::weak_ptr<std::decay_t<typename T::element_type>>>;

template <std_shared_ptr T>
struct make_weak_ref_fn<T> {
  template <class TValue>
    requires std::same_as<std::decay_t<T>, std::decay_t<TValue>>
  static constexpr auto make(TValue&& value) {
    return std::weak_ptr<std::decay_t<typename T::element_type>> {
      std::forward<TValue>(value)};
  }
};

template <std_weak_ptr T>
struct lock_weak_ref_fn<T> {
  static constexpr auto lock(auto&& value) {
    return value.lock();
  }
};
}// namespace OpenKneeboard::weak_refs_extensions

namespace OpenKneeboard::weak_refs {
static_assert(strong_ref<std::shared_ptr<int>>);
static_assert(!weak_ref<std::shared_ptr<int>>);

static_assert(!strong_ref<std::weak_ptr<int>>);
static_assert(weak_ref<std::weak_ptr<int>>);

// Make sure that weak_refs::make_weak_ref() ADL doesn't cause compile-time
// recursion
class Test : public std::enable_shared_from_this<Test> {
 public:
  Test() = delete;
  Test(Test&&) = delete;
  Test(const Test&) = delete;

  Test& operator=(Test&&) = delete;
  Test& operator=(const Test&) = delete;

  ~Test();
};

static_assert(strong_ref<std::shared_ptr<Test>>);
static_assert(!weak_ref<std::shared_ptr<Test>>);
static_assert(!strong_ref<std::weak_ptr<Test>>);
static_assert(weak_ref<std::weak_ptr<Test>>);
static_assert(convertible_to_weak_ref<std::shared_ptr<Test>>);
static_assert(convertible_to_weak_ref<std::weak_ptr<Test>>);

}// namespace OpenKneeboard::weak_refs