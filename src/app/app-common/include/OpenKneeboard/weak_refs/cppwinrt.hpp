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

#include <winrt/Windows.Foundation.h>

#include <memory>

namespace OpenKneeboard::weak_refs_adl_definitions {
template <class T>
concept cppwinrt_type = std::
  convertible_to<std::decay_t<T>, winrt::Windows::Foundation::IInspectable>;

template <class T>
concept cppwinrt_ptr = cppwinrt_type<std::decay_t<std::remove_pointer_t<T>>>;

template <class T>
concept cppwinrt_com_ptr
  = cppwinrt_ptr<std::decay_t<decltype(std::declval<T>().get())>>
  && std::same_as<
      std::decay_t<T>,
      winrt::com_ptr<std::decay_t<decltype(*std::declval<T>().get())>>>;

template <
  cppwinrt_type T,
  class = decltype(winrt::make_weak(std::declval<T>()))>
consteval std::true_type is_cppwinrt_strong_ref_fn(T&&);
consteval std::false_type is_cppwinrt_strong_ref_fn(...);

template <class T>
concept cppwinrt_strong_ref
  = (cppwinrt_type<T>
     && decltype(is_cppwinrt_strong_ref_fn(std::declval<T>()))::value)
  || cppwinrt_com_ptr<T>;

static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IInspectable>);
static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IStringable>);

template <class T>
concept cppwinrt_weak_ref
  = (cppwinrt_strong_ref<decltype(std::declval<T>().get())>
     || cppwinrt_com_ptr<decltype(std::declval<T>().get())>)
  && weak_refs_adl_detail::
    decays_to_same_as<T, decltype(winrt::make_weak(std::declval<T>().get()))>;

static_assert(!cppwinrt_strong_ref<int>);
static_assert(!cppwinrt_strong_ref<std::shared_ptr<int>>);
static_assert(!cppwinrt_weak_ref<std::weak_ptr<int>>);

template <cppwinrt_strong_ref T>
struct adl_make_weak_ref<T> {
  template <class TValue>
  static constexpr auto make(TValue&& value) {
    return winrt::make_weak(std::forward<TValue>(value));
  }
};

template <cppwinrt_weak_ref T>
struct adl_lock_weak_ref<T> {
  static constexpr auto lock(auto&& value) {
    return value.get();
  }
};

static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IInspectable>);
static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IStringable>);
static_assert(!cppwinrt_strong_ref<
              winrt::weak_ref<winrt::Windows::Foundation::IStringable>>);
static_assert(
  cppwinrt_weak_ref<winrt::weak_ref<winrt::Windows::Foundation::IStringable>>);

}// namespace OpenKneeboard::weak_refs_adl_definitions

namespace OpenKneeboard::weak_refs {
static_assert(strong_ref<winrt::Windows::Foundation::IStringable>);
static_assert(!weak_ref<winrt::Windows::Foundation::IStringable>);

static_assert(!strong_ref<decltype(winrt::make_weak(
                winrt::Windows::Foundation::IStringable {nullptr}))>);
static_assert(!strong_ref<winrt::Windows::Foundation::IStringable*>);
static_assert(weak_ref<decltype(winrt::make_weak(
                winrt::Windows::Foundation::IStringable {nullptr}))>);
}// namespace OpenKneeboard::weak_refs