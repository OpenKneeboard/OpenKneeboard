/*
 * OpenKneeboard
 *
 * Copyright (C) 2023 Fred Emmott <fred@fredemmott.com>
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

#include <shims/Windows.h>
#include <shims/winrt/base.h>

#include <FredEmmott/bindline.hpp>

#include <expected>
#include <functional>

namespace FredEmmott::winapi {

template <class TDerived>
struct noarg_bindable_t : FredEmmott::bindline_extension_api::bindable_t {
  template <class TFn>
  constexpr auto bind_to(TFn&& fn) const noexcept {
    return std::bind_front(
      []<class InvokeFn>(InvokeFn&& fn, auto... args) -> decltype(auto) {
        return TDerived::invoke(std::forward<InvokeFn>(fn), args...);
      },
      std::forward<TFn>(fn));
  }
};

template <class TDerived>
struct result_transform_t : noarg_bindable_t<TDerived> {
  template <class TFn>
  constexpr static decltype(auto) invoke(TFn&& fn, auto... args) {
    return TDerived::transform_result(std::forward<TFn>(fn)(args...));
  }

  static decltype(auto) transform_result(auto) = delete;
};

template <class THandle>
struct basic_returns_handle
  : result_transform_t<basic_returns_handle<THandle>> {
  constexpr static std::expected<THandle, HRESULT> transform_result(
    HANDLE handle) {
    if (handle == INVALID_HANDLE_VALUE || !handle) {
      return std::unexpected(HRESULT_FROM_WIN32(GetLastError()));
    }
    return THandle {handle};
  }
};

template <class TTraits>
struct or_throw : result_transform_t<or_throw<TTraits>> {
  template <class T>
  constexpr static decltype(auto) transform_result(T&& expected) {
    if (expected.has_value()) {
      return std::forward<T>(expected).value();
    }
    TTraits::throw_hresult(std::forward<T>(expected).error());
  }
};

struct or_default : result_transform_t<or_default> {
  template <class T>
  static constexpr T transform_result(std::expected<T, HRESULT>&& expected) {
    if (expected.has_value()) {
      return std::move(expected).value();
    }
    return T {};
  }
};

struct result_identity : result_transform_t<result_identity> {
  template <class T>
  static constexpr decltype(auto) transform_result(
    std::expected<T, HRESULT>&& expected) {
    return std::move(expected);
  }
};

struct raw_winapi_traits {
  using handle_type = HANDLE;
  using file_handle_type = HANDLE;

  // Error message marker
  struct winapi_does_not_use_exceptions_call_optional_value_instead {};
  static winapi_does_not_use_exceptions_call_optional_value_instead
  throw_hresult(const HRESULT)
    = delete;
};

struct winrt_winapi_traits {
  using handle_type = winrt::handle;
  using file_handle_type = winrt::file_handle;

  [[noreturn]] static void throw_hresult(const HRESULT hr) {
    OPENKNEEBOARD_ASSERT(FAILED(hr));
    winrt::check_hresult(hr);
    std::unreachable();
  };
};

template <class TTraits, class TErrorMapper = result_identity>
struct basic_winapi {
 public:
  using returns_handle = basic_returns_handle<typename TTraits::handle_type>;
  using returns_file_handle
    = basic_returns_handle<typename TTraits::file_handle_type>;

  using or_throw = basic_winapi<TTraits, or_throw<TTraits>>;
  using or_default = basic_winapi<TTraits, or_default>;
  using expected = basic_winapi<TTraits>;

  // We could avoid spelling out the arguments here with `static inline const
  // CreateEventW = ... pipeline
  //
  // Spelling them out here to keep the same parameter coercion rules, and for
  // better IDE support

  // HANDLE or NULL
  static constexpr auto CreateEventW(
    LPSECURITY_ATTRIBUTES lpEventAttributes,
    BOOL bManualReset,
    BOOL bInitialState,
    LPCWSTR lpName = nullptr) {
    return (&::CreateEventW | returns_handle() | TErrorMapper())(
      lpEventAttributes, bManualReset, bInitialState, lpName);
  }

  static constexpr auto CreateFileMappingW(
    HANDLE hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD flProtect,
    DWORD dwMaximumSizeHigh,
    DWORD dwMaximumSizeLow,
    LPCWSTR lpName = nullptr) {
    return (&::CreateFileMappingW | returns_handle() | TErrorMapper())(
      hFile,
      lpFileMappingAttributes,
      flProtect,
      dwMaximumSizeHigh,
      dwMaximumSizeLow,
      lpName);
  }

  static constexpr auto CreateMutexW(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    BOOL bInitialOwner = true,
    LPCWSTR lpName = nullptr) {
    return (&::CreateMutexW | returns_handle() | TErrorMapper())(
      lpMutexAttributes, bInitialOwner, lpName);
  }

  static constexpr auto CreateWaitableTimerW(
    LPSECURITY_ATTRIBUTES lpTimerAttributes,
    BOOL bManualReset,
    LPCWSTR lpTimerName = nullptr) {
    return (&::CreateWaitableTimerW | returns_handle() | TErrorMapper())(
      lpTimerAttributes, bManualReset, lpTimerName);
  }

  // HANDLE or INVALID_HANDLE_VALUE
  static constexpr auto CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL,
    HANDLE hTemplateFile = NULL) {
    return (&::CreateFileW | returns_file_handle() | TErrorMapper())(
      lpFileName,
      dwDesiredAccess,
      dwShareMode,
      lpSecurityAttributes,
      dwCreationDisposition,
      dwFlagsAndAttributes,
      hTemplateFile);
  }
  static constexpr auto CreateMailslotW(
    LPCWSTR lpName,
    DWORD nMaxMessageSize,
    DWORD lReadTimeout,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes = nullptr) {
    return (&::CreateMailslotW | returns_file_handle() | TErrorMapper())(
      lpName, nMaxMessageSize, lReadTimeout, lpSecurityAttributes);
  }
};

}// namespace FredEmmott::winapi

namespace OpenKneeboard {
using Win32 = ::FredEmmott::winapi::basic_winapi<
  ::FredEmmott::winapi::winrt_winapi_traits>;
};