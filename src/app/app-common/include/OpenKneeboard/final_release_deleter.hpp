// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <concepts>
#include <memory>

namespace OpenKneeboard {

/** A class with a C++/WinRT-style `final_release()` static method.
 *
 *     static OpenKneeboard::fire_and_forget final_release(std::unique_ptr<T>);
 *
 * Not currently requiring 'noexcept' as it doesn't have the expected effect in
 * coroutines in C++20.
 */
template <class T>
concept with_final_release = std::is_invocable_r_v<
  OpenKneeboard::fire_and_forget,
  decltype(&T::final_release),
  typename std::unique_ptr<T>>;

/** Reimplementation of C++/WinRT's `final_release` pattern.
 *
 * This effectively allows a courtine finalizer in non-WinRT types.
 *
 * T must satisfy the `with_final_release<T>` concept, so must implement this
 * static method:
 *
 *     static OpenKneeboard::fire_and_forget final_release(std::unique_ptr<T>);
 *
 * See
 * https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/details-about-destructors
 */
template <with_final_release T>
struct final_release_deleter {
  void operator()(T* p) { T::final_release(std::unique_ptr<T>(p)); }
};

template <with_final_release T>
std::shared_ptr<T> shared_with_final_release(T* p) {
  return std::shared_ptr<T>(p, final_release_deleter<T> {});
}

}// namespace OpenKneeboard
