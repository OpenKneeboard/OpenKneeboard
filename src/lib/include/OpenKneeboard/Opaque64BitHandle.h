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

#include <OpenKneeboard/config.h>

#include <concepts>
#include <functional>

namespace OpenKneeboard {

template <class T>
struct Opaque64BitHandle {
  uint64_t mRawValue {};

  constexpr Opaque64BitHandle() = default;
  constexpr Opaque64BitHandle(nullptr_t) {};
  constexpr explicit Opaque64BitHandle(uint64_t value) : mRawValue(value) {};

  constexpr Opaque64BitHandle(const Opaque64BitHandle<T>&) = default;
  constexpr Opaque64BitHandle(Opaque64BitHandle<T>&&) = default;

  constexpr Opaque64BitHandle<T>& operator=(const Opaque64BitHandle<T>&)
    = default;

  constexpr Opaque64BitHandle<T>& operator=(Opaque64BitHandle<T>&&) = default;
  constexpr operator bool() const {
    return mRawValue != 0;
  }
};

}// namespace OpenKneeboard

template <class T>
  requires std::derived_from<T, OpenKneeboard::Opaque64BitHandle<T>>
struct std::hash<T> {
  constexpr size_t operator()(const T& handle) const noexcept {
    return handle.mRawValue;
  }
};