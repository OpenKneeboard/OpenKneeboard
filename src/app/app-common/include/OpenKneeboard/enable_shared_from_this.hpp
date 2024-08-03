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

#include <concepts>
#include <memory>
#include <type_traits>

namespace OpenKneeboard {

/** Extension of `std::enable_shared_from_this()` with subclassing support.
 *
 * This allows descendants of non-template derived classes to get a correctly
 * typed pointer.
 */
template <class Base>
class enable_shared_from_this : public std::enable_shared_from_this<Base> {
 public:
  template <class Self>
  auto shared_from_this(this Self& self) {
    using decayed_base_t = std::enable_shared_from_this<Base>;
    using base_t = std::conditional_t<
      std::is_const_v<Self>,
      const decayed_base_t,
      decayed_base_t>;

    auto parent = static_cast<base_t&>(self).shared_from_this();
    return static_pointer_cast<std::remove_reference_t<Self>>(parent);
  }

  template <class Self>
  auto weak_from_this(this Self& self) {
    return std::weak_ptr {self.shared_from_this()};
  }
};
}// namespace OpenKneeboard