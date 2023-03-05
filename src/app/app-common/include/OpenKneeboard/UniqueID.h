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
#include <cstdint>
#include <functional>
#include <type_traits>

namespace OpenKneeboard {

class _UniqueIDImpl {
 protected:
  static uint64_t GetAndIncrementNextValue();
};

template <class T>
class UniqueIDBase : private _UniqueIDImpl {
 public:
  UniqueIDBase() {
    mValue = _UniqueIDImpl::GetAndIncrementNextValue();
  }
  UniqueIDBase(nullptr_t) {
    mValue = 0;
  }

  UniqueIDBase(const UniqueIDBase<T>&) = default;
  UniqueIDBase(UniqueIDBase<T>&&) = default;
  UniqueIDBase& operator=(const UniqueIDBase<T>&) = default;
  UniqueIDBase& operator=(UniqueIDBase<T>&&) = default;

  // Values *must not* be persisted and restored
  constexpr uint64_t GetTemporaryValue() const {
    if (mValue == 0) {
      OPENKNEEBOARD_BREAK;
    }
    return mValue;
  }

  static constexpr T FromTemporaryValue(uint64_t value) {
    T ret;
    ret.mValue = value;
    return ret;
  }

 private:
  uint64_t mValue;
};

template <class T>
constexpr bool operator==(
  const UniqueIDBase<T>& a,
  const std::type_identity_t<UniqueIDBase<T>>& b) {
  return a.GetTemporaryValue() == b.GetTemporaryValue();
}

template <class T>
constexpr bool operator==(const UniqueIDBase<T>& id, uint64_t value) {
  return value == id.GetTemporaryValue();
}

class UniqueID final : public UniqueIDBase<UniqueID> {};
static_assert(std::equality_comparable<UniqueID>);

class PageID final : public UniqueIDBase<PageID> {};

};// namespace OpenKneeboard

template <class T>
  requires std::derived_from<T, OpenKneeboard::UniqueIDBase<T>>
struct std::hash<T> {
  constexpr std::size_t operator()(const T& id) const noexcept {
    return static_cast<std::size_t>(id.GetTemporaryValue());
  }
};
