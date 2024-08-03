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

#include <wil/cppwinrt_helpers.h>

#include <OpenKneeboard/weak_refs/bind_front_adl.hpp>

#include <concepts>
#include <functional>

namespace OpenKneeboard::weak_refs::cppwinrt_detail {
template <class TContext, class TFn>
struct context_binder {
 public:
  using function_t = TFn;

  context_binder() = delete;
  template <class InitContext, class InitFn>
  context_binder(InitContext&& context, InitFn&& fn)
    : mContext(std::forward<InitContext>(context)),
      mFn(std::forward<InitFn>(fn)) {
  }

  // Not using perfect forwarding for the arguments as this is a coroutine - so
  // we **MUST** copy the arguments.
  template <class... UnboundArgs>
    requires std::invocable<TFn, UnboundArgs...>
  winrt::fire_and_forget operator()(UnboundArgs... unboundArgs) const {
    if constexpr (std::
                    same_as<std::decay_t<TContext>, winrt::apartment_context>) {
      co_await mContext;
    } else {
      co_await wil::resume_foreground(mContext);
    }
    std::invoke(mFn, unboundArgs...);
  }

 private:
  TContext mContext;
  TFn mFn;
};
}// namespace OpenKneeboard::weak_refs::cppwinrt_detail

namespace OpenKneeboard::weak_refs::cppwinrt {

template <class Context, class F>
auto bind_context(Context&& context, F&& f) {
  return cppwinrt_detail::
    context_binder<std::decay_t<Context>, std::decay_t<F>> {
      std::forward<Context>(context), std::forward<F>(f)};
}

template <class Context, class... Args>
auto bind_front_with_context(Context&& context, Args&&... args) {
  using namespace weak_refs::bind_front_adl;
  return bind_context(
    std::forward<Context>(context), bind_front(std::forward<Args>(args)...));
}

}// namespace OpenKneeboard::weak_refs::cppwinrt