// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/config.hpp>

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
  UniqueIDBase() { mValue = _UniqueIDImpl::GetAndIncrementNextValue(); }
  constexpr UniqueIDBase(nullptr_t) { mValue = 0; }

  UniqueIDBase(const UniqueIDBase<T>&) = default;
  UniqueIDBase(UniqueIDBase<T>&&) = default;
  UniqueIDBase& operator=(const UniqueIDBase<T>&) = default;
  UniqueIDBase& operator=(UniqueIDBase<T>&&) = default;

  constexpr operator bool() const noexcept { return mValue != 0; }

  // Values *must not* be persisted and restored
  constexpr uint64_t GetTemporaryValue() const noexcept { return mValue; }

  static constexpr T FromTemporaryValue(uint64_t value) noexcept {
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
static_assert(sizeof(UniqueID) == sizeof(uint64_t));
static_assert(std::is_trivially_copyable_v<UniqueID>);

class PageID final : public UniqueIDBase<PageID> {};

};// namespace OpenKneeboard

template <class T>
  requires std::derived_from<T, OpenKneeboard::UniqueIDBase<T>>
struct std::hash<T> {
  constexpr std::size_t operator()(const T& id) const noexcept {
    if (!id) {
      OPENKNEEBOARD_BREAK;
    }
    return static_cast<std::size_t>(id.GetTemporaryValue());
  }
};
