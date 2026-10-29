// Copyright (c) 2026 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/fatal.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace OpenKneeboard {

/** A dynamic-length fixed-capacity fixed-layout contiguous container.
 *
 * This is intended to:
 * - avoid heap allocations
 * - be suitable for storing in shared memory that is used for IPC between
 *   32-bit and 64-bit processes
 */
template <class T, std::size_t N>
struct inplace_vector {
  static_assert(N > 0, "inplace_vector must have a positive capacity");

  using value_type = T;
  // not using std::size_t because that would vary between 32-bit and 64-bit
  // consumers
  using size_type = decltype([] constexpr {
    if constexpr (N <= 0xff) {
      return std::type_identity<uint8_t> {};
    } else if constexpr (N <= 0xffff) {
      return std::type_identity<uint16_t> {};
    } else if constexpr (N <= 0xffff'ffff) {
      return std::type_identity<uint32_t> {};
    } else {
      return std::type_identity<uint64_t> {};
    }
  }())::type;
  using iterator = T*;
  using const_iterator = const T*;

  constexpr inplace_vector() = default;

  template <std::input_iterator It>
  constexpr inplace_vector(It first, It last) {
    for (auto it = first; it != last; ++it) {
      emplace_back(*it);
    }
  }

  constexpr inplace_vector(inplace_vector&& other) noexcept {
    move_from(other);
  }

  inplace_vector& operator=(inplace_vector&& other) noexcept {
    move_from(other);
    return *this;
  }

  constexpr ~inplace_vector() { clear(); }

  constexpr void clear() { resize(0); }

  constexpr void resize(const size_type count, const T& init = T {}) {
    if (count > N) [[unlikely]] {
      throw std::out_of_range(
        std::format(
          "inplace_vector::resize: count {} exceeds capacity {}", count, N));
    }

    if (count == mSize) {
      return;
    }

    if (count < mSize) {
      for (size_type i = count; i < mSize; ++i) {
        std::destroy_at(&mStorage[i].value);
      }
      mSize = count;
      return;
    }

    for (size_type i = mSize; i < count; ++i) {
      emplace_back(init);
    }
  }

  template <class... Args>
  constexpr T& emplace_back(Args&&... args) {
    if (mSize == N) [[unlikely]] {
      throw std::out_of_range("inplace_vector::emplace_back");
    }
    const auto it =
      std::construct_at(&mStorage[mSize].value, std::forward<Args>(args)...);
    ++mSize;
    return *it;
  }

  constexpr T& push_back(const T& value) { return emplace_back(value); }
  constexpr T& push_back(T&& value) { return emplace_back(std::move(value)); }

  constexpr T& operator[](const size_type index) noexcept {
    OPENKNEEBOARD_ASSERT(index < mSize);
    return mStorage[index].value;
  }
  constexpr const T& operator[](const size_type index) const noexcept {
    OPENKNEEBOARD_ASSERT(index < mSize);
    return mStorage[index].value;
  }

  constexpr T& at(const size_type index) {
    check_index(index);
    return mStorage[index].value;
  }

  constexpr const T& at(const size_type index) const {
    check_index(index);
    return mStorage[index].value;
  }

  constexpr T& front() noexcept {
    OPENKNEEBOARD_ASSERT(mSize > 0);
    return mStorage[0].value;
  }
  constexpr T& back() noexcept {
    OPENKNEEBOARD_ASSERT(mSize > 0);
    return mStorage[mSize - 1].value;
  }
  constexpr const T& front() const noexcept {
    OPENKNEEBOARD_ASSERT(mSize > 0);
    return mStorage[0].value;
  }
  constexpr const T& back() const noexcept {
    OPENKNEEBOARD_ASSERT(mSize > 0);
    return mStorage[mSize - 1].value;
  }

  // std::ranges::range
  constexpr iterator begin() noexcept { return data(); }
  constexpr iterator end() noexcept { return begin() + mSize; }
  constexpr const_iterator cbegin() const noexcept { return data(); }
  constexpr const_iterator cend() const noexcept { return cbegin() + mSize; }
  constexpr const_iterator begin() const noexcept { return cbegin(); }
  constexpr const_iterator end() const noexcept { return cbegin() + mSize; }

  // std::ranges::sized_range
  constexpr std::size_t capacity() const noexcept { return N; }
  constexpr auto size() const noexcept { return mSize; }
  [[nodiscard]]
  constexpr bool empty() const noexcept {
    return mSize == 0;
  }

  // std::ranges::contiguous_range
  [[nodiscard]]
  constexpr T* data() noexcept {
    if (empty()) {
      return nullptr;
    }
    return &mStorage[0].value;
  }
  [[nodiscard]]
  constexpr const T* data() const noexcept {
    if (empty()) {
      return nullptr;
    }
    return &mStorage[0].value;
  }

  constexpr inplace_vector(const inplace_vector& other)
    : inplace_vector(other.cbegin(), other.cend()) {}

  constexpr inplace_vector& operator=(const inplace_vector& other) {
    if (this == std::addressof(other)) {
      return *this;
    }

    const auto minSize = std::min(mSize, other.mSize);
    for (size_type i = 0; i < minSize; ++i) {
      mStorage[i].value = other.mStorage[i].value;
    }
    for (size_type i = minSize; i < mSize; ++i) {
      std::destroy_at(&mStorage[i].value);
    }
    mSize = minSize;
    for (size_type i = minSize; i < other.mSize; ++i) {
      std::construct_at(&mStorage[i].value, other.mStorage[i].value);
      // Do this after each element so that mSize is still correct if the
      // constructor throws
      ++mSize;
    }

    return *this;
  }

 private:
  union storage_type {
    T value;
    char dummy;
    constexpr storage_type() : dummy() {}
    constexpr ~storage_type() { /* caller must manually cleanup */ }
    storage_type(const storage_type&) = delete;
    storage_type(storage_type&&) = delete;
    storage_type& operator=(const storage_type&) = delete;
    storage_type& operator=(storage_type&&) = delete;
  };

  alignas(T) storage_type mStorage[N] {};
  size_type mSize {};

  void check_index(const size_type index) const {
    if (index >= mSize) [[unlikely]] {
      throw std::out_of_range(
        std::format("index {} out of range [0, {})", index, mSize));
    }
  }

  void move_from(inplace_vector<T, N>& other) noexcept {
    if (this == std::addressof(other)) {
      return;
    }

    clear();

    for (size_type i = 0; i < other.mSize; ++i) {
      emplace_back(std::move(other.mStorage[i].value));
    }

    other.clear();
  }
};
}// namespace OpenKneeboard
