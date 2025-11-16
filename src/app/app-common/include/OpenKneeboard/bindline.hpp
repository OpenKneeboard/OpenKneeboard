// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/cppwinrt.hpp>
#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <FredEmmott/bindline.hpp>

#include <utility>

namespace OpenKneeboard {

namespace bindline = FredEmmott::bindline;
using namespace bindline;

template <::FredEmmott::weak_refs::weak_ref... TWeak>
struct task_front_refs_binder_t
  : public ::FredEmmott::bindline_extension_api::bindable_t {
  using ordering_t
    = ::FredEmmott::bindline_extension_api::ordering_requirements_t;
  static constexpr ordering_t ordering_requirements_v
    = ordering_t::invoke_after_context_switch;

  template <FredEmmott::weak_refs::convertible_to_weak_ref... TInit>
  task_front_refs_binder_t(TInit&&... inits)
    : mWeaks(std::decay_t<TInit> {std::forward<TInit>(inits)}...) {
  }

  template <class Self, class TFn>
  constexpr auto bind_to(this const Self& self, TFn&& fn) {
    return std::bind_front(
      []<class... Unbound>(
        auto weaks, auto fn, Unbound&&... unbound) -> task<void> {
        co_await Self::invoke(weaks, fn, std::forward<Unbound>(unbound)...);
      },
      self.mWeaks,
      std::decay_t<TFn> {std::forward<TFn>(fn)});
  }

 private:
  template <class... Unbound>
  static task<void>
  invoke(std::tuple<TWeak...> weaks, auto fn, Unbound&&... unbound) {
    auto strong = std::apply(
      []<class... CallArgs>(CallArgs&&... weaks) {
        return std::tuple {
          ::FredEmmott::weak_refs::lock_weak_ref(
            std::forward<CallArgs>(weaks))...,
        };
      },
      weaks);
    const auto all_live = std::apply(
      []<class... CallArgs>(CallArgs&&... strongs) {
        return (static_cast<bool>(strongs) && ...);
      },
      strong);
    if (!all_live) {
      co_return;
    }
    co_await std::apply(
      fn,
      std::tuple_cat(strong, std::forward_as_tuple<Unbound...>(unbound...)));
  }

  std::tuple<TWeak...> mWeaks;
};

template <class... TArgs>
[[nodiscard]]
constexpr auto task_bind_refs_front(TArgs&&... args) {
  namespace wr = ::FredEmmott::weak_refs;
  return ::FredEmmott::bindline_extension_api::bindable_t::make_projected<
    task_front_refs_binder_t>(
    []<wr::convertible_to_weak_ref T>(T&& v) constexpr {
      return wr::make_weak_ref(std::forward<T>(v));
    },
    std::forward<TArgs>(args)...);
}

template <class TQueue>
struct ordered_enqueue_binder_t
  : public ::FredEmmott::bindline_extension_api::bindable_t {
  using ordering_t
    = ::FredEmmott::bindline_extension_api::ordering_requirements_t;
  static constexpr ordering_t ordering_requirements_v
    = ordering_t::invoke_after_context_switch;

  ordered_enqueue_binder_t() = delete;
  constexpr ordered_enqueue_binder_t(TQueue* kbs) : mKneeboard(kbs) {
  }

  template <class TFn>
  constexpr auto bind_to(TFn&& fn) const {
    return std::bind_front(
      &TQueue::EnqueueOrderedEvent, mKneeboard, std::decay_t<TFn> {fn});
  }

 private:
  TQueue* mKneeboard {nullptr};
};

class KneeboardState;

constexpr auto bind_enqueue(KneeboardState* kneeboard) {
  return ordered_enqueue_binder_t {kneeboard};
}

template <class TFn>
constexpr auto bind_enqueue(KneeboardState* kneeboard, TFn&& fn) {
  return ordered_enqueue_binder_t {kneeboard}.bind_to(std::forward<TFn>(fn));
}

}// namespace OpenKneeboard