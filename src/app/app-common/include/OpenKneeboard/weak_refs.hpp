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

/*
 * Helpers for writing templates that deal with both
 * std::weak_ptr and winrt::weak_ref.
 *
 * # Concepts
 *
 * - weak_ref_or_ptr: either std::weak_ptr or winrt::weak_ref
 * - strong_ref: the result of locking either
 *
 * # Types
 *
 * - strong_t<weak>: tnhe corresponding strong type
 *
 * # Functions
 *
 * - lock_weak(): locks a weak_ref_or_ptr
 * - weak_ref(): creates a weak_ref_or_ptr
 * - weak_refs(): creates a tuple of weak_ref_or_ptr
 * - lock_weaks(): creates an std::optional of strong references
 */

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <memory>
#include <optional>

namespace OpenKneeboard {

// Marker for functions to automatically convert strong to weak and back again,
// e.g. for EventHandlers
class auto_weak_ref_t final {};
constexpr auto_weak_ref_t auto_weak_ref {};

namespace detail {

template <class T>
struct weak_ref_traits;

template <class T>
struct weak_ref_traits<std::weak_ptr<T>> {
  static auto lock(std::weak_ptr<T> weak) {
    return weak.lock();
  }

  using strong_type = std::shared_ptr<T>;
};

template <class T>
struct weak_ref_traits<winrt::weak_ref<T>> {
  static auto lock(winrt::weak_ref<T> weak) {
    return weak.get();
  }

  using strong_type = decltype(lock(std::declval<winrt::weak_ref<T>>()));
};

}// namespace detail

template <class T>
concept weak_ref_or_ptr = requires(T a) {
  {
    detail::weak_ref_traits<T>::lock(a)
  } -> std::convertible_to<typename detail::weak_ref_traits<T>::strong_type>;
};

static_assert(weak_ref_or_ptr<std::weak_ptr<int>>);
static_assert(!weak_ref_or_ptr<std::shared_ptr<int>>);
static_assert(!weak_ref_or_ptr<int*>);

template <weak_ref_or_ptr Weak>
auto lock_weak(Weak weak) {
  return detail::weak_ref_traits<Weak>::lock(weak);
}

template <weak_ref_or_ptr T>
using strong_t = typename detail::weak_ref_traits<T>::strong_type;
namespace detail {

template <class T>
auto convert_to_weak(const std::shared_ptr<T>& p) {
  return std::weak_ptr(p);
}

template <class T>
auto convert_to_weak(std::enable_shared_from_this<T>* p) {
  return p->weak_from_this();
}

template <class T>
concept cpp_winrt_this_ptr = requires(T a) {
  { a->get_weak() } -> weak_ref_or_ptr;
};

template <cpp_winrt_this_ptr T>
auto convert_to_weak(T p) {
  return p->get_weak();
}

template <std::derived_from<winrt::Windows::Foundation::IInspectable> T>
auto convert_to_weak(const T& o) {
  return winrt::make_weak(o);
}

}// namespace detail

template <class T>
concept strong_ref = requires(T a) {
  { detail::convert_to_weak(a) } -> weak_ref_or_ptr;
};

template <strong_ref T>
auto weak_ref(T a) {
  return detail::convert_to_weak(a);
}

template <strong_ref First, strong_ref... Rest>
auto weak_refs(First first, Rest... rest) {
  if constexpr (sizeof...(rest) == 0) {
    return std::make_tuple(weak_ref(first));
  } else {
    return std::tuple_cat(std::make_tuple(weak_ref(first)), weak_refs(rest...));
  }
}

template <weak_ref_or_ptr First, weak_ref_or_ptr... Rest>
auto lock_weaks(First first, Rest... rest) {
  if constexpr (sizeof...(rest) == 0) {
    auto strong = lock_weak(first);
    if (strong) {
      return std::optional(std::make_tuple(strong));
    }
    return std::optional<std::tuple<strong_t<First>>>(std::nullopt);
  } else {
    auto head = lock_weaks(first);
    if (head) {
      auto tail = lock_weaks(rest...);
      if (tail) {
        return std::optional(std::tuple_cat(*head, *tail));
      }
    }
    return std::optional<std::tuple<strong_t<First>, strong_t<Rest>...>>(
      std::nullopt);
  }
}

template <weak_ref_or_ptr... Args>
auto lock_weaks(std::tuple<Args...> tuple) {
  auto impl = [](Args... args) { return lock_weaks(args...); };
  return std::apply(impl, tuple);
}

}// namespace OpenKneeboard