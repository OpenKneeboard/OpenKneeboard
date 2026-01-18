// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/config.hpp>

#include <felly/numeric_cast.hpp>

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
    return felly::numeric_cast<size_t>(handle.mRawValue);
  }
};