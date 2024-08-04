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

#include "bind_maybe_refs_front.hpp"

#include <tuple>
#include <utility>

namespace OpenKneeboard::weak_refs {

template <class F, convertible_to_weak_ref... Binds>
auto bind_refs_front(F&& f, Binds&&... binds) {
  const auto ret = bind_maybe_refs_detail::front_binder<
    bind_maybe_refs_detail::NotARefBehavior::Error,
    std::decay_t<F>,
    std::decay_t<Binds>...>(std::forward<F>(f), std::forward<Binds>(binds)...);
  static_assert(decltype(ret)::refs_required_v);
  return ret;
}

}// namespace OpenKneeboard::weak_refs