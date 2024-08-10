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

#include <shims/winrt/base.h>

#include <FredEmmott/composable_bind.hpp>
#include <OpenKneeboard/cppwinrt.hpp>

#include <utility>

namespace OpenKneeboard {
using namespace FredEmmott::composable_bind;

// TODO (C++26): Delet This.
//
// Use parameter pack indexing instead.
template <size_t... I>
struct drop_last_impl_t {
  drop_last_impl_t() = delete;
  constexpr drop_last_impl_t(std::index_sequence<I...>) {
  }

  template <template <class...> class T, class... TArgs>
  using next_t = T<std::tuple_element_t<I, std::tuple<TArgs...>>...>;

  template <class TIn>
  static constexpr decltype(auto) next_tuple(TIn&& in) {
    return std::forward_as_tuple(
      std::forward<std::tuple_element_t<I, TIn>>(get<I>(in))...);
  }
};

template <class... TArgs>
struct drop_last_seq_t {
  template <class TFirst, class... TRest>
  static std::index_sequence_for<TRest...> seq_t_fn(TFirst&&, TRest&&...);
  static std::index_sequence<> seq_t_fn();

  using type = decltype(seq_t_fn(std::declval<TArgs>()...));
};

struct drop_last_t {
  template <class... TArgs>
  using seq_t = drop_last_seq_t<TArgs...>::type;

  template <class... TArgs>
  using impl_t = decltype(drop_last_impl_t(seq_t<TArgs...> {}));

  template <template <class...> class T, class... TArgs>
  using next_t = impl_t<TArgs...>::template next_t<T, TArgs...>;

  template <size_t ToDrop = 1, class... TArgs>
  static constexpr decltype(auto) next_tuple(std::tuple<TArgs...>&& args) {
    static_assert(ToDrop > 0);
    auto drop_one = impl_t<TArgs...>::next_tuple(std::move(args));
    if constexpr (ToDrop == 1) {
      return drop_one;
    } else {
      return drop_last_t::next_tuple<ToDrop - 1>(std::move(drop_one));
    }
  }

  template <size_t ToDrop = 1, class... TArgs>
  static constexpr decltype(auto) make_tuple(TArgs&&... args) {
    static_assert(ToDrop > 0);
    return drop_last_t::next_tuple<ToDrop>(
      std::forward_as_tuple(std::forward<TArgs>(args)...));
  }
};

static_assert(drop_last_t::make_tuple(1, 2, 3) == std::tuple {1, 2});
static_assert(drop_last_t::make_tuple<1>(1, 2, 3) == std::tuple {1, 2});
static_assert(drop_last_t::make_tuple<2>(1, 2, 3) == std::tuple {1});
static_assert(drop_last_t::make_tuple<3>(1, 2, 3) == std::tuple {});

template <class TFn, class... TArgs>
struct arg_dropping_invocable_t {
  static consteval bool value_fn() {
    if constexpr (std::invocable<TFn, TArgs...>) {
      return true;
    } else if constexpr (sizeof...(TArgs) >= 1) {
      using next_t
        = drop_last_t::template next_t<arg_dropping_invocable_t, TFn, TArgs...>;
      return next_t::value_fn();
    }
    return false;
  }

  static constexpr bool value = value_fn();
};

template <class TFn, class... TArgs>
concept arg_dropping_invocable = arg_dropping_invocable_t<TFn, TArgs...>::value;

template <class TFn, class... TArgs>
  requires arg_dropping_invocable<TFn, TArgs...>
constexpr decltype(auto) arg_dropping_invoke(TFn&& fn, TArgs&&... args) {
  if constexpr (std::invocable<TFn, TArgs...>) {
    return std::invoke(std::forward<TFn>(fn), std::forward<TArgs>(args)...);
  } else if constexpr (sizeof...(TArgs) >= 1) {
    auto next_args = drop_last_t::make_tuple(
      std::forward<TFn>(fn), std::forward<TArgs>(args)...);
    // no `- 1` as `args` excludes TFn, `next-args` includes it
    static_assert(std::tuple_size_v<decltype(next_args)> == sizeof...(TArgs));
    return std::apply(
      []<class... NextArgs>(NextArgs&&... args) -> decltype(auto) {
        return arg_dropping_invoke(std::forward<NextArgs>(args)...);
      },
      std::move(next_args));
  } else {
    static_assert(false, "Should be statically unreachable due to concept");
  }
}

constexpr auto test_fn = [](int a, int b) { return a + b; };

static_assert(arg_dropping_invocable<decltype(test_fn), int, int>);
static_assert(arg_dropping_invocable<decltype(test_fn), int, int, int>);
static_assert(arg_dropping_invocable<decltype(test_fn), int, int, int, int>);
static_assert(arg_dropping_invocable<decltype(test_fn), int, int, void*>);
static_assert(arg_dropping_invocable<decltype(test_fn), int, int, float*>);
static_assert(!arg_dropping_invocable<decltype(test_fn), int>);
static_assert(!arg_dropping_invocable<decltype(test_fn), int, float*>);

constexpr int it = 2;

static_assert(arg_dropping_invoke(test_fn, 1, 2) == 3);
static_assert(arg_dropping_invoke(test_fn, 1, 2, 3) == 3);
static_assert(arg_dropping_invoke(test_fn, 1, 2, it) == 3);
static_assert(arg_dropping_invoke(test_fn, 1, it, 3) == 3);
static_assert(arg_dropping_invoke(test_fn, 1, 2, 3, 4) == 3);
static_assert(arg_dropping_invoke(test_fn, 1, 2, "i am not an int") == 3);
static_assert(arg_dropping_invoke(test_fn, 1, 2, nullptr) == 3);
static_assert(arg_dropping_invoke(test_fn, 1, 2, nullptr, "lolwhut") == 3);

static_assert(arg_dropping_invoke(test_fn | bind_front(1), 2, 3) == 3);

namespace {
struct ADITestClass {
  void Foo0() {};
  void Foo1(int) {};
  int Foo2(int a, int b) {
    return a + b;
  }
  void Foo3(int, int, int) {
  }

 private:
  int mMarker {123};
};
void static_adi_test() {
  auto it = std::make_shared<ADITestClass>();
  using TFoo0 = decltype(&ADITestClass::Foo0);
  using TFoo2 = decltype(&ADITestClass::Foo2);
  using TStrong = decltype(it);
  static_assert(arg_dropping_invocable<TFoo0, TStrong>);
  static_assert(arg_dropping_invocable<TFoo0, TStrong, int>);
  static_assert(arg_dropping_invocable<TFoo2, TStrong, int, int>);
  static_assert(arg_dropping_invocable<TFoo2, TStrong, int, int, int>);
}
}// namespace

struct drop_winrt_event_args_t : bindable_t {
  template <class TFn>
  [[nodiscard]]
  constexpr auto bind_to(TFn&& fn) const {
    return [fn = TFn {std::forward<TFn>(fn)}]<class... TArgs>(TArgs&&... args) {
      return invoke(fn, std::forward<TArgs>(args)...);
    };
  };

 private:
  template <class... TArgs>
  static decltype(auto) make_tuple(TArgs&&... args) {
    return drop_last_t::make_tuple<2>(std::forward<TArgs>(args)...);
  }

  template <class TFn, class... TArgs>
    requires(sizeof...(TArgs) >= 2)
    && winrt_type<
              std::tuple_element_t<sizeof...(TArgs) - 1, std::tuple<TArgs...>>>
    && winrt_type<
              std::tuple_element_t<sizeof...(TArgs) - 2, std::tuple<TArgs...>>>
  static auto invoke(TFn&& fn, TArgs&&... args) {
    return std::apply(
      std::forward<TFn>(fn), make_tuple(std::forward<TArgs>(args)...));
  }
};

[[nodiscard]]
constexpr auto drop_winrt_event_args() {
  return drop_winrt_event_args_t {};
}

template <class TFn>
[[nodiscard]]
constexpr auto drop_winrt_event_args(TFn&& fn) {
  return drop_winrt_event_args().bind_to(std::forward<TFn>(fn));
}

}// namespace OpenKneeboard