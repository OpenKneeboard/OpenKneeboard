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

#include <winrt/Windows.Foundation.h>

namespace OpenKneeboard::cppwinrt::inline cppwinrt_concepts {
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

template <class T>
concept cppwinrt_strong_ref = cppwinrt_com_ptr<T>
  || (cppwinrt_type<T> && requires(T v) { winrt::make_weak(v); });

static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IInspectable>);
static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IStringable>);

template <class T>
concept cppwinrt_weak_ref
  = (cppwinrt_strong_ref<decltype(std::declval<T>().get())>
     || cppwinrt_com_ptr<decltype(std::declval<T>().get())>)
  && std::same_as<
      std::decay_t<T>,
      std::decay_t<decltype(winrt::make_weak(std::declval<T>().get()))>>;

template <class T>
concept cppwinrt_raw_pointer = std::is_pointer_v<T> && requires(T v) {
  { v->get_weak() } -> cppwinrt_weak_ref;
};

static_assert(!cppwinrt_strong_ref<int>);
static_assert(!cppwinrt_strong_ref<std::shared_ptr<int>>);
static_assert(!cppwinrt_weak_ref<std::weak_ptr<int>>);

static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IInspectable>);
static_assert(cppwinrt_strong_ref<winrt::Windows::Foundation::IStringable>);
static_assert(!cppwinrt_strong_ref<
              winrt::weak_ref<winrt::Windows::Foundation::IStringable>>);
static_assert(
  cppwinrt_weak_ref<winrt::weak_ref<winrt::Windows::Foundation::IStringable>>);

}// namespace OpenKneeboard::cppwinrt::inline cppwinrt_concepts