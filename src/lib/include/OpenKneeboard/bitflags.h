#pragma once

#include <type_traits>

namespace OpenKneeboard {

/// Specialize this for your enum class to enable operator overloads
template <class T>
  requires std::is_enum_v<T>
constexpr bool is_bitflags_v = false;

template <class T>
concept bitflags = std::is_enum_v<T> && is_bitflags_v<T>;

/// Helper to allow (flags & Foo::Bar) pattern
template <bitflags T>
class CoerceableBitflags final {
 private:
  T value;

 public:
  constexpr CoerceableBitflags(T v) : value(v) {
  }

  constexpr operator T() const {
    return value;
  }

  constexpr operator bool() const {
    return static_cast<bool>(value);
  }
};

template <bitflags T>
constexpr T operator|(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) | static_cast<UT>(b));
}

template <bitflags T>
constexpr CoerceableBitflags<T> operator&(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) & static_cast<UT>(b));
}

template <bitflags T>
constexpr T operator^(T a, T b) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<UT>(a) ^ static_cast<UT>(b));
}

template <bitflags T>
constexpr T operator~(T a) {
  using UT = std::underlying_type_t<T>;
  return static_cast<T>(~static_cast<UT>(a));
}

template <bitflags T>
constexpr T& operator|=(T& a, const T& b) {
  return a = (a | b);
}

template <bitflags T>
constexpr T& operator&=(T& a, const T& b) {
  return a = (a & b);
}

template <bitflags T>
constexpr T& operator^=(T& a, const T& b) {
  return a = (a ^ b);
}

}// namespace OpenKneeboard
